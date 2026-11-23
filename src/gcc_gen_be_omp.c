/**
 * @file gcc_gen_be_omp.c
 * @brief Implementation of OpenMP directive lowering to the GNU `libgomp` runtime ABI.
 * @details Parses OpenMP `#pragma omp` directives and clauses (`num_threads`, `if`, `schedule`,
 * `collapse`, `private`, `firstprivate`, `lastprivate`, `shared`, `reduction`, `nowait`, `filter`),
 * outlines `parallel` and `task` bodies into internal `gcc_jit_function` callbacks taking a captured
 * context struct pointer, and emits GNU `libgomp` calls (`GOMP_parallel`, `GOMP_loop_*_start` /
 * `GOMP_loop_*_next` / `GOMP_loop_end`, `GOMP_barrier`, `GOMP_critical_*`, `GOMP_single_start`,
 * `omp_get_thread_num`, `GOMP_task`, `GOMP_taskwait`, and `GOMP_atomic_start`/`end`).
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_omp.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_atomic.h"
#include <libgccjit.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Supported OpenMP work-sharing loop schedule kinds.
 */
typedef enum gcc_be_omp_sched_kind {
    /** @brief Static work-sharing schedule (`GOMP_loop_static_*`). */
    GCC_BE_OMP_SCHED_STATIC = 0,
    /** @brief Dynamic work-sharing schedule (`GOMP_loop_dynamic_*`). */
    GCC_BE_OMP_SCHED_DYNAMIC = 1,
    /** @brief Guided work-sharing schedule (`GOMP_loop_guided_*`). */
    GCC_BE_OMP_SCHED_GUIDED = 2,
    /** @brief Runtime-selected work-sharing schedule (`GOMP_loop_runtime_*`). */
    GCC_BE_OMP_SCHED_RUNTIME = 3
} gcc_be_omp_sched_kind_t;

/**
 * @brief Supported OpenMP reduction operators.
 */
typedef enum gcc_be_omp_red_op {
    /** @brief Not a reduction variable. */
    GCC_BE_OMP_RED_NONE = 0,
    /** @brief Additive reduction (`+` or `-`, identity `0`). */
    GCC_BE_OMP_RED_ADD = 1,
    /** @brief Multiplicative reduction (`*`, identity `1`). */
    GCC_BE_OMP_RED_MUL = 2,
    /** @brief Bitwise AND reduction (`&`, identity `~0`). */
    GCC_BE_OMP_RED_BITAND = 3,
    /** @brief Bitwise OR reduction (`|`, identity `0`). */
    GCC_BE_OMP_RED_BITOR = 4,
    /** @brief Bitwise XOR reduction (`^`, identity `0`). */
    GCC_BE_OMP_RED_BITXOR = 5,
    /** @brief Logical AND reduction (`&&`, identity `1`). */
    GCC_BE_OMP_RED_LOGAND = 6,
    /** @brief Logical OR reduction (`||`, identity `0`). */
    GCC_BE_OMP_RED_LOGOR = 7,
    /** @brief Minimum value reduction (`min`, identity max representable). */
    GCC_BE_OMP_RED_MIN = 8,
    /** @brief Maximum value reduction (`max`, identity min representable). */
    GCC_BE_OMP_RED_MAX = 9
} gcc_be_omp_red_op_t;

/**
 * @brief Data-sharing classification for a variable referenced inside an OpenMP region.
 */
typedef enum gcc_be_omp_var_sharing {
    /** @brief Shared reference via pointer field in the outlined context struct. */
    GCC_BE_OMP_SHARE_SHARED = 0,
    /** @brief Uninitialized private local variable inside the outlined region. */
    GCC_BE_OMP_SHARE_PRIVATE = 1,
    /** @brief Private local initialized from outer value via context struct field. */
    GCC_BE_OMP_SHARE_FIRSTPRIVATE = 2,
    /** @brief Private local written back to outer variable on the final logical iteration. */
    GCC_BE_OMP_SHARE_LASTPRIVATE = 3,
    /** @brief Private accumulator combined atomically into outer variable at region exit. */
    GCC_BE_OMP_SHARE_REDUCTION = 4
} gcc_be_omp_var_sharing_t;

/**
 * @brief Binding between an EDG statement (or scope) and a `#pragma omp` directive text.
 */
typedef struct gcc_be_omp_pragma_entry {
    /** @brief Associated statement pointer (`NULL` when trailing at end of `owner_scope`). */
    a_statement_ptr stmt;
    /** @brief Enclosing scope for trailing standalone pragmas (`NULL` when `stmt != NULL`). */
    a_scope_ptr owner_scope;
    /** @brief Owned directive string (`"#pragma omp ..."` or `"omp ..."`). */
    char pragma_text[512];
    /** @brief `TRUE` once consumed by statement or scope-exit lowering. */
    a_boolean consumed;
} gcc_be_omp_pragma_entry_t;

/**
 * @brief Named variable clause item parsed from `private(...)`, `reduction(...)`, etc.
 */
typedef struct gcc_be_omp_clause_var {
    /** @brief Identifier spelling inside the OpenMP clause. */
    char name[64];
    /** @brief Data-sharing mode assigned by the clause. */
    gcc_be_omp_var_sharing_t sharing;
    /** @brief Reduction operator when `sharing == GCC_BE_OMP_SHARE_REDUCTION`. */
    gcc_be_omp_red_op_t red_op;
} gcc_be_omp_clause_var_t;

/**
 * @brief Parsed representation of an OpenMP `#pragma omp ...` directive and its clauses.
 */
typedef struct gcc_be_omp_parsed_dir {
    /** @brief `TRUE` for `parallel` (including `parallel for` and `parallel for simd`). */
    a_boolean is_parallel;
    /** @brief `TRUE` for work-sharing `for` (`for`, `parallel for`, `for simd`, `parallel for simd`). */
    a_boolean is_for;
    /** @brief `TRUE` for `simd` (`simd`, `for simd`, `parallel for simd`). */
    a_boolean is_simd;
    /** @brief `TRUE` for standalone `barrier`. */
    a_boolean is_barrier;
    /** @brief `TRUE` for `critical`. */
    a_boolean is_critical;
    /** @brief `TRUE` for `single`. */
    a_boolean is_single;
    /** @brief `TRUE` for `master` or `masked`. */
    a_boolean is_master_or_masked;
    /** @brief `TRUE` for `task`. */
    a_boolean is_task;
    /** @brief `TRUE` for standalone `taskwait`. */
    a_boolean is_taskwait;
    /** @brief `TRUE` for `atomic`. */
    a_boolean is_atomic;
    /** @brief Optional `atomic` sub-clause (`0`=update, `1`=read, `2`=write, `3`=capture). */
    int atomic_mode;
    /** @brief `TRUE` when `nowait` clause is present. */
    a_boolean has_nowait;
    /** @brief `TRUE` when `if(0)` / `if(false)` forces serialized execution. */
    a_boolean if_is_false;
    /** @brief Requested `num_threads(N)` constant (`0` for runtime default). */
    unsigned int num_threads;
    /** @brief Loop `collapse(K)` depth (`>= 1`). */
    int collapse_count;
    /** @brief Thread ID filter for `master`/`masked` (`0` by default). */
    int filter_tid;
    /** @brief Work-sharing schedule kind for `#pragma omp for`. */
    gcc_be_omp_sched_kind_t sched_kind;
    /** @brief Schedule chunk size (`0` when unspecified). */
    long chunk_size;
    /** @brief Optional critical section name (`""` for anonymous critical lock). */
    char critical_name[64];
    /** @brief Parsed variable data-sharing / reduction clauses (up to 32 entries). */
    gcc_be_omp_clause_var_t clause_vars[32];
    /** @brief Number of valid entries in `clause_vars`. */
    int num_clause_vars;
} gcc_be_omp_parsed_dir_t;

/**
 * @brief Descriptor for a single captured local variable in an outlined OpenMP region.
 */
typedef struct gcc_be_omp_capture {
    /** @brief Frontend variable pointer. */
    a_variable_ptr var;
    /** @brief Outer function's `gcc_jit_lvalue *` prior to outlining. */
    gcc_jit_lvalue *outer_lval;
    /** @brief Lowered `gcc_jit_type *` of `var`. */
    gcc_jit_type *val_type;
    /** @brief Data-sharing mode inside the outlined callback. */
    gcc_be_omp_var_sharing_t sharing;
    /** @brief Reduction operator when `sharing == GCC_BE_OMP_SHARE_REDUCTION`. */
    gcc_be_omp_red_op_t red_op;
    /** @brief Struct field in `__edg_omp_ctx_N` (`NULL` for pure `private` variables). */
    gcc_jit_field *ctx_field;
    /** @brief Private or reduction accumulator local inside the outlined function. */
    gcc_jit_lvalue *inner_local;
} gcc_be_omp_capture_t;

/**
 * @brief Extracted canonical bounds and step of a single `stmk_for` OpenMP loop level.
 */
typedef struct gcc_be_omp_loop_level {
    /** @brief Loop induction variable (`a_variable_ptr`). */
    a_variable_ptr iv_var;
    /** @brief Lowered `gcc_jit_lvalue *` of `iv_var` in the active function. */
    gcc_jit_lvalue *iv_lval;
    /** @brief Lowered `gcc_jit_type *` of `iv_var`. */
    gcc_jit_type *iv_jit_type;
    /** @brief Initial bound expression `lb` (`iv = lb`), or `NULL` when `lb_const != NULL`. */
    an_expr_node_ptr lb_expr;
    /** @brief Constant initial bound (`for (int i = C; ...)`) when `dip->kind == dik_constant`. */
    a_constant_ptr lb_const;
    /** @brief Termination bound expression `ub` (`iv < ub`, `<=`, `>`, `>=`). */
    an_expr_node_ptr ub_expr;
    /** @brief Step delta expression (`NULL` when unit step `+1` / `-1` from `++` / `--`). */
    an_expr_node_ptr step_expr;
    /** @brief Comparison operator kind (`eok_lt`, `eok_le`, `eok_gt`, `eok_ge`, `eok_ne`). */
    an_expr_operator_kind cmp_op;
    /** @brief `+1` for upward step (`+=`, `++`), `-1` for downward step (`-=`, `--`). */
    int step_sign;
    /** @brief `TRUE` if `iv_var` has an unsigned 64-bit integer type. */
    a_boolean is_unsigned_64;
    /** @brief Loop body statement of this level. */
    a_statement_ptr body_stmt;
} gcc_be_omp_loop_level_t;

/**
 * @brief Skips leading ASCII whitespace and locates the `"omp"` directive payload if present.
 * @details Accepts `"#pragma omp ..."`, `"pragma omp ..."`, or `"omp ..."` with arbitrary spacing.
 * @param[in] raw_text Candidate pragma string (may be NULL).
 * @param[out] out_payload Pointer receiving the position immediately past `"omp"`, or `NULL` if not OpenMP.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t locate_omp_payload(
    const char *raw_text,
    const char **out_payload) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_payload = NULL;
    const char *p = raw_text;
    while (isspace((unsigned char)*p)) ++p;
    if (*p == '#') {
        ++p;
        while (isspace((unsigned char)*p)) ++p;
    }
    if (strncmp(p, "pragma", 6) == 0 && isspace((unsigned char)p[6])) {
        p += 6;
        while (isspace((unsigned char)*p)) ++p;
    }
    if (strncmp(p, "omp", 3) == 0 && (p[3] == '\0' || isspace((unsigned char)p[3]))) {
        p += 3;
        while (isspace((unsigned char)*p)) ++p;
        *out_payload = p;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_bind_omp_pragma(
    a_statement_ptr target_stmt,
    a_scope_ptr owner_scope,
    const char *pragma_text) GCC_GEN_BE_NOEXCEPT {
    if (!pragma_text) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    const char *omp_payload = NULL;
    GCC_GEN_BE_CHECK(locate_omp_payload(pragma_text, &omp_payload));
    if (!omp_payload) {
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_gen_be_context_t *state = &gcc_gen_be_singleton_state;
    state->openmp_enabled = TRUE;
    gcc_be_omp_pragma_entry_t *arr = (gcc_be_omp_pragma_entry_t *)state->omp_pragma_entries;
    for (size_t i = 0; i < state->omp_pragma_count; ++i) {
        if (!arr[i].consumed && arr[i].stmt == target_stmt &&
            arr[i].owner_scope == owner_scope &&
            strncmp(arr[i].pragma_text, pragma_text, sizeof(arr[i].pragma_text) - 1U) == 0) {
            return GCC_GEN_BE_SUCCESS;
        }
    }

    if (state->omp_pragma_count >= state->omp_pragma_capacity) {
        size_t new_cap = (state->omp_pragma_capacity == 0) ? 16U : (state->omp_pragma_capacity * 2U);
        void *resized = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(
            state->omp_pragma_entries, new_cap * sizeof(gcc_be_omp_pragma_entry_t), &resized));
        state->omp_pragma_entries = resized;
        state->omp_pragma_capacity = new_cap;
        arr = (gcc_be_omp_pragma_entry_t *)resized;
    }

    size_t idx = state->omp_pragma_count++;
    arr[idx].stmt = target_stmt;
    arr[idx].owner_scope = owner_scope;
    snprintf(arr[idx].pragma_text, sizeof(arr[idx].pragma_text), "%s", pragma_text);
    arr[idx].consumed = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Reads the closest preceding `#pragma omp` directive line before `stmt_line` in `file_path`.
 * @details Scans lines `1 .. stmt_line - 1`, skipping blank lines and single-line comments after the
 * directive, and copies any `#pragma omp ...` line immediately preceding `stmt_line` into `out_line`.
 * @param[in] file_path Source file path resolved by `conv_seq_to_file_and_line`.
 * @param[in] stmt_line 1-based physical line number of the candidate statement.
 * @param[out] out_line Output buffer receiving the `#pragma omp ...` line (`""` when none precedes `stmt_line`).
 * @param[in] out_cap Capacity of `out_line` in bytes.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t read_preceding_omp_line(
    const char *file_path,
    a_line_number stmt_line,
    char *out_line,
    size_t out_cap) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    out_line[0] = '\0';
    if (file_path[0] == '\0') {
        return GCC_GEN_BE_SUCCESS;
    }
    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        return GCC_GEN_BE_SUCCESS;
    }
    char raw[512];
    char last_nonblank[512];
    last_nonblank[0] = '\0';
    a_line_number cur_line = 1;
    while (cur_line < stmt_line && fgets(raw, (int)sizeof(raw), fp) != NULL) {
        const char *s = raw;
        while (isspace((unsigned char)*s)) ++s;
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len - 1])) {
            --len;
        }
        if (len > 0 && !(len >= 2 && s[0] == '/' && s[1] == '/')) {
            size_t copy_n = (len < sizeof(last_nonblank) - 1U) ? len : (sizeof(last_nonblank) - 1U);
            memcpy(last_nonblank, s, copy_n);
            last_nonblank[copy_n] = '\0';
        }
        ++cur_line;
    }
    fclose(fp);
    const char *payload = NULL;
    GCC_GEN_BE_CHECK(locate_omp_payload(last_nonblank, &payload));
    if (payload && last_nonblank[0] == '#') {
        snprintf(out_line, out_cap, "%s", last_nonblank);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively walks an EDG statement tree to bind source `#pragma omp` lines to statements.
 * @details Uses `conv_seq_to_file_and_line` on each statement with a distinct physical line so
 * `#pragma omp` directives are discovered even when `INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL` is disabled.
 * @param[in] stmt_list Head of the statement list to scan.
 * @param[in,out] claimed_lines Array of 1-based physical line numbers already claimed in this routine.
 * @param[in,out] inout_claimed_count Active count of entries in `claimed_lines`.
 * @param[in] claimed_cap Capacity of `claimed_lines`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t scan_omp_pragmas_in_stmt_tree(
    a_statement_ptr stmt_list,
    a_line_number *claimed_lines,
    size_t *inout_claimed_count,
    size_t claimed_cap) GCC_GEN_BE_NOEXCEPT {
    for (a_statement_ptr cur = stmt_list; cur != NULL; cur = cur->next) {
        a_seq_number eff_seq = cur->position.seq;
        if (eff_seq == 0 && cur->kind == stmk_block && cur->variant.block.statements) {
            a_statement_ptr fs = cur->variant.block.statements;
            while (fs && (fs->kind == stmk_decl || fs->kind == stmk_empty)) {
                fs = fs->next;
            }
            if (fs && fs->kind == stmk_init && fs->next && fs->next->kind == stmk_for && fs->next->next == NULL) {
                eff_seq = (fs->position.seq != 0) ? fs->position.seq : fs->next->position.seq;
            }
        }
        if (eff_seq != 0) {
            a_const_char *short_fn = NULL;
            a_const_char *full_fn = NULL;
            a_line_number phys_line = 0;
            a_boolean at_eos = FALSE;
            conv_seq_to_file_and_line(eff_seq, &short_fn, &full_fn, &phys_line, &at_eos);
            const char *src_path = (full_fn && full_fn[0] != '\0') ? full_fn : short_fn;
            a_boolean already_claimed = FALSE;
            for (size_t i = 0; i < *inout_claimed_count; ++i) {
                if (claimed_lines[i] == phys_line) {
                    already_claimed = TRUE;
                    break;
                }
            }
            if (!already_claimed && phys_line > 1 && src_path) {
                if (*inout_claimed_count < claimed_cap) {
                    claimed_lines[(*inout_claimed_count)++] = phys_line;
                }
                char omp_line[512];
                GCC_GEN_BE_CHECK(read_preceding_omp_line(src_path, phys_line, omp_line, sizeof(omp_line)));
                if (omp_line[0] != '\0') {
                    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(cur, NULL, omp_line));
                }
            }
        }
        if (cur->kind == stmk_block) {
            GCC_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(
                cur->variant.block.statements, claimed_lines, inout_claimed_count, claimed_cap));
        } else if (cur->kind == stmk_for) {
            GCC_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(
                cur->variant.for_loop.statement, claimed_lines, inout_claimed_count, claimed_cap));
        } else if (cur->kind == stmk_while || cur->kind == stmk_end_test_while) {
            GCC_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(
                cur->variant.loop_statement, claimed_lines, inout_claimed_count, claimed_cap));
        } else if (cur->kind == stmk_if) {
            GCC_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(
                cur->variant.if_stmt.then_statement, claimed_lines, inout_claimed_count, claimed_cap));
            GCC_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(
                cur->variant.if_stmt.else_statement, claimed_lines, inout_claimed_count, claimed_cap));
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_register_omp_pragmas_in_scope(
    a_scope_ptr scope) GCC_GEN_BE_NOEXCEPT {
    if (!scope) {
        return GCC_GEN_BE_SUCCESS;
    }
    for (a_pragma_ptr p = scope->pragmas; p != NULL; p = p->next) {
        if (p->pragma_text) {
            a_statement_ptr bound_stmt = (p->entity.kind == iek_statement && p->entity.ptr)
                ? (a_statement_ptr)p->entity.ptr
                : NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(bound_stmt, bound_stmt ? NULL : scope, p->pragma_text));
        }
    }
    if (gcc_gen_be_singleton_state.openmp_enabled && scope->assoc_block) {
        a_line_number claimed_lines[256];
        size_t claimed_count = 0U;
        GCC_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(
            scope->assoc_block, claimed_lines, &claimed_count, 256U));
    }
    for (a_scope_ptr sub = scope->scopes; sub != NULL; sub = sub->next) {
        GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(sub));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Reads the next identifier or keyword token from `*inout_cur` into `out_tok`.
 * @details Advances `*inout_cur` past leading whitespace and copies `[A-Za-z0-9_]` characters.
 * @param[in,out] inout_cur Cursor pointer into the directive text.
 * @param[out] out_tok Buffer receiving the identifier token (`out_cap` bytes).
 * @param[in] out_cap Size of `out_tok` in bytes.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t scan_omp_ident(
    const char **inout_cur,
    char *out_tok,
    size_t out_cap) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    const char *p = *inout_cur;
    while (isspace((unsigned char)*p)) ++p;
    size_t k = 0;
    while (isalnum((unsigned char)*p) || *p == '_') {
        if (k + 1 < out_cap) {
            out_tok[k++] = *p;
        }
        ++p;
    }
    out_tok[k] = '\0';
    *inout_cur = p;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Records a variable name and sharing/reduction mode in `dir->clause_vars`.
 * @details Updates an existing entry if `vname` was already listed (e.g. `firstprivate` + `lastprivate`).
 * @param[in,out] dir Parsed OpenMP directive structure.
 * @param[in] vname Identifier name string.
 * @param[in] sharing Data-sharing mode.
 * @param[in] red_op Reduction operator (`GCC_BE_OMP_RED_NONE` when not a reduction).
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t record_omp_clause_var(
    gcc_be_omp_parsed_dir_t *dir,
    const char *vname,
    gcc_be_omp_var_sharing_t sharing,
    gcc_be_omp_red_op_t red_op) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (vname[0] == '\0') return GCC_GEN_BE_SUCCESS;
    for (int i = 0; i < dir->num_clause_vars; ++i) {
        if (strcmp(dir->clause_vars[i].name, vname) == 0) {
            dir->clause_vars[i].sharing = sharing;
            dir->clause_vars[i].red_op = red_op;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (dir->num_clause_vars < 32) {
        int idx = dir->num_clause_vars++;
        snprintf(dir->clause_vars[idx].name, sizeof(dir->clause_vars[idx].name), "%s", vname);
        dir->clause_vars[idx].sharing = sharing;
        dir->clause_vars[idx].red_op = red_op;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Parses a comma-separated `(v1, v2, ...)` variable list inside an OpenMP data-sharing clause.
 * @details Advances `*inout_cur` past the closing `)` delimiter and populates `dir->clause_vars`.
 * @param[in,out] inout_cur Cursor positioned at the opening `(` of the clause.
 * @param[in,out] dir Parsed OpenMP directive descriptor.
 * @param[in] sharing Data-sharing mode to assign.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t parse_omp_var_list_clause(
    const char **inout_cur,
    gcc_be_omp_parsed_dir_t *dir,
    gcc_be_omp_var_sharing_t sharing) GCC_GEN_BE_NOEXCEPT {
    const char *p = *inout_cur;
    while (isspace((unsigned char)*p)) ++p;
    if (*p != '(') {
        *inout_cur = p;
        return GCC_GEN_BE_SUCCESS;
    }
    ++p;
    while (*p && *p != ')') {
        char vname[64];
        GCC_GEN_BE_CHECK(scan_omp_ident(&p, vname, sizeof(vname)));
        GCC_GEN_BE_CHECK(record_omp_clause_var(dir, vname, sharing, GCC_BE_OMP_RED_NONE));
        while (*p && *p != ',' && *p != ')') ++p;
        if (*p == ',') ++p;
    }
    if (*p == ')') ++p;
    *inout_cur = p;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Parses a `reduction(op : v1, v2, ...)` OpenMP clause.
 * @details Recognizes `+`, `-`, `*`, `&`, `|`, `^`, `&&`, `||`, `min`, and `max` reduction operators.
 * @param[in,out] inout_cur Cursor positioned at the opening `(` of `reduction(...)`.
 * @param[in,out] dir Parsed OpenMP directive descriptor.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t parse_omp_reduction_clause(
    const char **inout_cur,
    gcc_be_omp_parsed_dir_t *dir) GCC_GEN_BE_NOEXCEPT {
    const char *p = *inout_cur;
    while (isspace((unsigned char)*p)) ++p;
    if (*p != '(') {
        *inout_cur = p;
        return GCC_GEN_BE_SUCCESS;
    }
    ++p;
    while (isspace((unsigned char)*p)) ++p;

    gcc_be_omp_red_op_t rop = GCC_BE_OMP_RED_ADD;
    if (strncmp(p, "&&", 2) == 0) {
        rop = GCC_BE_OMP_RED_LOGAND;
        p += 2;
    } else if (strncmp(p, "||", 2) == 0) {
        rop = GCC_BE_OMP_RED_LOGOR;
        p += 2;
    } else if (strncmp(p, "min", 3) == 0 && !isalnum((unsigned char)p[3])) {
        rop = GCC_BE_OMP_RED_MIN;
        p += 3;
    } else if (strncmp(p, "max", 3) == 0 && !isalnum((unsigned char)p[3])) {
        rop = GCC_BE_OMP_RED_MAX;
        p += 3;
    } else if (*p == '+' || *p == '-') {
        rop = GCC_BE_OMP_RED_ADD;
        ++p;
    } else if (*p == '*') {
        rop = GCC_BE_OMP_RED_MUL;
        ++p;
    } else if (*p == '&') {
        rop = GCC_BE_OMP_RED_BITAND;
        ++p;
    } else if (*p == '|') {
        rop = GCC_BE_OMP_RED_BITOR;
        ++p;
    } else if (*p == '^') {
        rop = GCC_BE_OMP_RED_BITXOR;
        ++p;
    }
    while (*p && *p != ':' && *p != ')') ++p;
    if (*p == ':') ++p;

    while (*p && *p != ')') {
        char vname[64];
        GCC_GEN_BE_CHECK(scan_omp_ident(&p, vname, sizeof(vname)));
        GCC_GEN_BE_CHECK(record_omp_clause_var(dir, vname, GCC_BE_OMP_SHARE_REDUCTION, rop));
        while (*p && *p != ',' && *p != ')') ++p;
        if (*p == ',') ++p;
    }
    if (*p == ')') ++p;
    *inout_cur = p;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Parses a full `#pragma omp ...` directive string into `gcc_be_omp_parsed_dir_t`.
 * @details Extracts construct keywords (`parallel`, `for`, `simd`, `barrier`, `critical`, `single`,
 * `master`, `masked`, `task`, `taskwait`, `atomic`) and clauses (`schedule`, `collapse`,
 * `num_threads`, `if`, `nowait`, `filter`, `private`, `firstprivate`, `lastprivate`, `shared`, `reduction`).
 * @param[in] pragma_text Raw `#pragma omp ...` string.
 * @param[out] out_dir Pointer receiving the parsed directive descriptor.
 * @return `GCC_GEN_BE_SUCCESS` on valid OpenMP directive, or `GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t parse_omp_directive(
    const char *pragma_text,
    gcc_be_omp_parsed_dir_t *out_dir) GCC_GEN_BE_NOEXCEPT {
    memset(out_dir, 0, sizeof(*out_dir));
    out_dir->collapse_count = 1;
    out_dir->sched_kind = GCC_BE_OMP_SCHED_STATIC;

    const char *p = NULL;
    GCC_GEN_BE_CHECK(locate_omp_payload(pragma_text, &p));
    if (*p == '\0') {
        return GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED;
    }

    /* Parse leading construct keywords */
    for (;;) {
        const char *save = p;
        char kw[64];
        GCC_GEN_BE_CHECK(scan_omp_ident(&p, kw, sizeof(kw)));
        if (kw[0] == '\0') break;
        if (strcmp(kw, "parallel") == 0) {
            out_dir->is_parallel = TRUE;
        } else if (strcmp(kw, "for") == 0) {
            out_dir->is_for = TRUE;
        } else if (strcmp(kw, "simd") == 0) {
            out_dir->is_simd = TRUE;
        } else if (strcmp(kw, "barrier") == 0) {
            out_dir->is_barrier = TRUE;
            break;
        } else if (strcmp(kw, "critical") == 0) {
            out_dir->is_critical = TRUE;
            while (isspace((unsigned char)*p)) ++p;
            if (*p == '(') {
                ++p;
                GCC_GEN_BE_CHECK(scan_omp_ident(&p, out_dir->critical_name, sizeof(out_dir->critical_name)));
                while (*p && *p != ')') ++p;
                if (*p == ')') ++p;
            }
            break;
        } else if (strcmp(kw, "single") == 0) {
            out_dir->is_single = TRUE;
            break;
        } else if (strcmp(kw, "master") == 0 || strcmp(kw, "masked") == 0) {
            out_dir->is_master_or_masked = TRUE;
            break;
        } else if (strcmp(kw, "task") == 0) {
            out_dir->is_task = TRUE;
            break;
        } else if (strcmp(kw, "taskwait") == 0) {
            out_dir->is_taskwait = TRUE;
            break;
        } else if (strcmp(kw, "atomic") == 0) {
            out_dir->is_atomic = TRUE;
            break;
        } else {
            p = save;
            break;
        }
    }

    if (!out_dir->is_parallel && !out_dir->is_for && !out_dir->is_simd &&
        !out_dir->is_barrier && !out_dir->is_critical && !out_dir->is_single &&
        !out_dir->is_master_or_masked && !out_dir->is_task && !out_dir->is_taskwait &&
        !out_dir->is_atomic) {
        return GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED;
    }

    /* Parse optional clauses */
    while (*p != '\0') {
        while (isspace((unsigned char)*p) || *p == ',') ++p;
        if (*p == '\0') break;
        char cl[64];
        GCC_GEN_BE_CHECK(scan_omp_ident(&p, cl, sizeof(cl)));
        if (cl[0] == '\0') {
            ++p;
            continue;
        }
        if (strcmp(cl, "nowait") == 0) {
            out_dir->has_nowait = TRUE;
        } else if (out_dir->is_atomic && strcmp(cl, "read") == 0) {
            out_dir->atomic_mode = 1;
        } else if (out_dir->is_atomic && strcmp(cl, "write") == 0) {
            out_dir->atomic_mode = 2;
        } else if (out_dir->is_atomic && strcmp(cl, "capture") == 0) {
            out_dir->atomic_mode = 3;
        } else if (out_dir->is_atomic && strcmp(cl, "update") == 0) {
            out_dir->atomic_mode = 0;
        } else if (strcmp(cl, "private") == 0) {
            GCC_GEN_BE_CHECK(parse_omp_var_list_clause(&p, out_dir, GCC_BE_OMP_SHARE_PRIVATE));
        } else if (strcmp(cl, "firstprivate") == 0) {
            GCC_GEN_BE_CHECK(parse_omp_var_list_clause(&p, out_dir, GCC_BE_OMP_SHARE_FIRSTPRIVATE));
        } else if (strcmp(cl, "lastprivate") == 0 || strcmp(cl, "linear") == 0) {
            GCC_GEN_BE_CHECK(parse_omp_var_list_clause(&p, out_dir, GCC_BE_OMP_SHARE_LASTPRIVATE));
        } else if (strcmp(cl, "shared") == 0) {
            GCC_GEN_BE_CHECK(parse_omp_var_list_clause(&p, out_dir, GCC_BE_OMP_SHARE_SHARED));
        } else if (strcmp(cl, "reduction") == 0) {
            GCC_GEN_BE_CHECK(parse_omp_reduction_clause(&p, out_dir));
        } else if (strcmp(cl, "num_threads") == 0 || strcmp(cl, "collapse") == 0 ||
                   strcmp(cl, "filter") == 0 || strcmp(cl, "if") == 0) {
            while (isspace((unsigned char)*p)) ++p;
            if (*p == '(') {
                ++p;
                while (isspace((unsigned char)*p)) ++p;
                long v = strtol(p, NULL, 10);
                if (strcmp(cl, "num_threads") == 0 && v > 0) {
                    out_dir->num_threads = (unsigned int)v;
                } else if (strcmp(cl, "collapse") == 0 && v >= 1) {
                    out_dir->collapse_count = (int)v;
                } else if (strcmp(cl, "filter") == 0) {
                    out_dir->filter_tid = (int)v;
                } else if (strcmp(cl, "if") == 0 && (*p == '0' || strncmp(p, "false", 5) == 0)) {
                    out_dir->if_is_false = TRUE;
                }
                int depth = 1;
                while (*p && depth > 0) {
                    if (*p == '(') ++depth;
                    else if (*p == ')') --depth;
                    ++p;
                }
            }
        } else if (strcmp(cl, "schedule") == 0) {
            while (isspace((unsigned char)*p)) ++p;
            if (*p == '(') {
                ++p;
                char sk[32];
                GCC_GEN_BE_CHECK(scan_omp_ident(&p, sk, sizeof(sk)));
                if (strcmp(sk, "monotonic") == 0 || strcmp(sk, "nonmonotonic") == 0) {
                    while (*p == ':' || isspace((unsigned char)*p)) ++p;
                    GCC_GEN_BE_CHECK(scan_omp_ident(&p, sk, sizeof(sk)));
                }
                if (strcmp(sk, "dynamic") == 0) out_dir->sched_kind = GCC_BE_OMP_SCHED_DYNAMIC;
                else if (strcmp(sk, "guided") == 0) out_dir->sched_kind = GCC_BE_OMP_SCHED_GUIDED;
                else if (strcmp(sk, "runtime") == 0 || strcmp(sk, "auto") == 0) out_dir->sched_kind = GCC_BE_OMP_SCHED_RUNTIME;
                else out_dir->sched_kind = GCC_BE_OMP_SCHED_STATIC;
                while (isspace((unsigned char)*p)) ++p;
                if (*p == ',') {
                    ++p;
                    long ch = strtol(p, NULL, 10);
                    if (ch > 0) out_dir->chunk_size = ch;
                }
                while (*p && *p != ')') ++p;
                if (*p == ')') ++p;
            }
        } else {
            while (isspace((unsigned char)*p)) ++p;
            if (*p == '(') {
                int depth = 1;
                ++p;
                while (*p && depth > 0) {
                    if (*p == '(') ++depth;
                    else if (*p == ')') --depth;
                    ++p;
                }
            }
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Declares or looks up an external GNU `libgomp` runtime function in `GCC_GEN_BE_CACHE_FUNC`.
 * @details Caches imported `GOMP_*` and `omp_get_*` declarations per `gcc_jit_context` so multiple
 * OpenMP regions in the same TU reuse identical `gcc_jit_function *` handles.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] sym_name External `libgomp` C ABI name (e.g., `"GOMP_parallel"`).
 * @param[in] ret_type Return `gcc_jit_type *`.
 * @param[in] num_params Number of parameter types in `param_types`.
 * @param[in] param_types Array of parameter `gcc_jit_type *` (may be NULL when `num_params == 0`).
 * @param[out] out_fn Pointer receiving the imported `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t get_gomp_runtime_fn(
    gcc_jit_context *ctx,
    const char *sym_name,
    gcc_jit_type *ret_type,
    int num_params,
    gcc_jit_type **param_types,
    gcc_jit_function **out_fn) GCC_GEN_BE_NOEXCEPT {
    void *cached = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)sym_name, &cached));
    if (cached) {
        *out_fn = (gcc_jit_function *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_param *params[12];
    for (int i = 0; i < num_params; ++i) {
        char pname[24];
        snprintf(pname, sizeof(pname), "gp_%d", i);
        params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], pname);
    }
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, sym_name, num_params, num_params > 0 ? params : NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)sym_name, fn));
    *out_fn = fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a zero-argument `void` GNU `libgomp` synchronization call (`GOMP_barrier`, `GOMP_taskwait`, etc.).
 * @details Resolves `sym_name` via `get_gomp_runtime_fn` and appends the call to the current block.
 * @param[in] sym_name External `libgomp` symbol (`"GOMP_barrier"`, `"GOMP_taskwait"`, etc.).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t emit_gomp_void_call(const char *sym_name) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx || !cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *fn = NULL;
    GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, sym_name, void_t, 0, NULL, &fn));
    gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, fn, 0, NULL));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively walks an EDG expression tree to collect outer local variables referenced inside an OpenMP body.
 * @details Only captures variables already bound to a `gcc_jit_lvalue *` in `GCC_GEN_BE_CACHE_VAR` whose
 * storage class is automatic/parameter (`!is_global`), deduplicating by `a_variable_ptr`.
 * @param[in] expr Frontend expression node (may be NULL).
 * @param[in,out] vars Output buffer of captured `a_variable_ptr` entries.
 * @param[in,out] inout_count Current count of entries in `vars` (capped at `max_vars`).
 * @param[in] max_vars Capacity of `vars`.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t collect_vars_in_expr(
    an_expr_node_ptr expr,
    a_variable_ptr *vars,
    int *inout_count,
    int max_vars) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    for (an_expr_node_ptr e = expr; e != NULL; e = e->next) {
        if (e->kind == enk_variable && e->variant.variable.ptr) {
            a_variable_ptr v = e->variant.variable.ptr;
            if (v->storage_class != sc_static && v->storage_class != sc_extern) {
                void *bound = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, v, &bound));
                if (bound) {
                    a_boolean seen = FALSE;
                    for (int i = 0; i < *inout_count; ++i) {
                        if (vars[i] == v) {
                            seen = TRUE;
                            break;
                        }
                    }
                    if (!seen && *inout_count < max_vars) {
                        vars[(*inout_count)++] = v;
                    }
                }
            }
        } else if (e->kind == enk_operation) {
            GCC_GEN_BE_CHECK(collect_vars_in_expr(e->variant.operation.operands, vars, inout_count, max_vars));
        } else if (e->kind == enk_condition && e->variant.condition) {
            GCC_GEN_BE_CHECK(collect_vars_in_expr(e->variant.condition->expr, vars, inout_count, max_vars));
        } else if (e->kind == enk_temp_init && e->variant.init.dynamic_init &&
                   e->variant.init.dynamic_init->kind == dik_expression) {
            GCC_GEN_BE_CHECK(collect_vars_in_expr(e->variant.init.dynamic_init->variant.expression, vars, inout_count, max_vars));
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t collect_vars_in_stmt_list(
    a_statement_ptr list,
    a_variable_ptr *vars,
    int *inout_count,
    int max_vars) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Walks a single EDG statement subtree to collect outer local variables referenced inside an OpenMP region.
 * @details Traverses blocks, loops, conditionals, and dynamic initializers without walking sibling `stmt->next`.
 * @param[in] s Root statement of the OpenMP construct (may be NULL).
 * @param[in,out] vars Output buffer of captured `a_variable_ptr` entries.
 * @param[in,out] inout_count Current count of entries in `vars`.
 * @param[in] max_vars Capacity of `vars`.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t collect_vars_in_stmt(
    a_statement_ptr s,
    a_variable_ptr *vars,
    int *inout_count,
    int max_vars) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!s) return GCC_GEN_BE_SUCCESS;
    if (s->kind == stmk_block) {
        return collect_vars_in_stmt_list(s->variant.block.statements, vars, inout_count, max_vars);
    }
    if (s->kind == stmk_if) {
        GCC_GEN_BE_CHECK(collect_vars_in_expr(s->expr, vars, inout_count, max_vars));
        GCC_GEN_BE_CHECK(collect_vars_in_stmt(s->variant.if_stmt.then_statement, vars, inout_count, max_vars));
        return collect_vars_in_stmt(s->variant.if_stmt.else_statement, vars, inout_count, max_vars);
    }
    if (s->kind == stmk_while || s->kind == stmk_end_test_while) {
        GCC_GEN_BE_CHECK(collect_vars_in_expr(s->expr, vars, inout_count, max_vars));
        return collect_vars_in_stmt(s->variant.loop_statement, vars, inout_count, max_vars);
    }
    if (s->kind == stmk_for) {
        if (s->variant.for_loop.extra_info) {
            GCC_GEN_BE_CHECK(collect_vars_in_stmt_list(s->variant.for_loop.extra_info->initialization, vars, inout_count, max_vars));
            GCC_GEN_BE_CHECK(collect_vars_in_expr(s->variant.for_loop.extra_info->increment, vars, inout_count, max_vars));
        }
        GCC_GEN_BE_CHECK(collect_vars_in_expr(s->expr, vars, inout_count, max_vars));
        return collect_vars_in_stmt(s->variant.for_loop.statement, vars, inout_count, max_vars);
    }
    if (s->kind == stmk_init) {
        if (s->variant.dynamic_init && s->variant.dynamic_init->kind == dik_expression) {
            return collect_vars_in_expr(s->variant.dynamic_init->variant.expression, vars, inout_count, max_vars);
        }
        return GCC_GEN_BE_SUCCESS;
    }
    return collect_vars_in_expr(s->expr, vars, inout_count, max_vars);
}

/**
 * @brief Walks statements inside a block list (`stmk_block.statements` chain).
 * @details Visits every sibling statement inside a compound block when gathering OpenMP captures.
 * @param[in] list Head of a statement list.
 * @param[in,out] vars Output variable array.
 * @param[in,out] inout_count Current count of entries in `vars`.
 * @param[in] max_vars Capacity of `vars`.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t collect_vars_in_stmt_list(
    a_statement_ptr list,
    a_variable_ptr *vars,
    int *inout_count,
    int max_vars) GCC_GEN_BE_NOEXCEPT {
    for (a_statement_ptr s = list; s != NULL; s = s->next) {
        GCC_GEN_BE_CHECK(collect_vars_in_stmt(s, vars, inout_count, max_vars));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Produces the OpenMP reduction identity rvalue for `red_op` and scalar `val_type`.
 * @details Emits `0` (`+`, `-`, `|`, `^`, `||`), `1` (`*`, `&&`), `~0` (`&`), max value (`min`), or min value (`max`).
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] edg_type Frontend variable `a_type_ptr`.
 * @param[in] val_type Lowered `gcc_jit_type *`.
 * @param[in] red_op OpenMP reduction operator.
 * @param[out] out_ident Pointer receiving the identity `gcc_jit_rvalue *`.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t make_omp_reduction_identity(
    gcc_jit_context *ctx,
    a_type_ptr edg_type,
    gcc_jit_type *val_type,
    gcc_be_omp_red_op_t red_op,
    gcc_jit_rvalue **out_ident) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    a_type_ptr ut = edg_type;
    while (ut->kind == tk_typeref) ut = ut->variant.typeref.type;
    a_boolean is_flt = (ut->kind == tk_float) ? TRUE : FALSE;

    switch (red_op) {
        case GCC_BE_OMP_RED_MUL:
        case GCC_BE_OMP_RED_LOGAND:
            *out_ident = is_flt
                ? gcc_jit_context_new_rvalue_from_double(ctx, val_type, 1.0)
                : gcc_jit_context_one(ctx, val_type);
            break;
        case GCC_BE_OMP_RED_BITAND:
            *out_ident = gcc_jit_context_new_unary_op(
                ctx, NULL, GCC_JIT_UNARY_OP_BITWISE_NEGATE, val_type, gcc_jit_context_zero(ctx, val_type));
            break;
        case GCC_BE_OMP_RED_MIN:
            *out_ident = is_flt
                ? gcc_jit_context_new_rvalue_from_double(ctx, val_type, 1.0e300)
                : gcc_jit_context_new_rvalue_from_long(ctx, val_type, 0x7fffffffL);
            break;
        case GCC_BE_OMP_RED_MAX:
            *out_ident = is_flt
                ? gcc_jit_context_new_rvalue_from_double(ctx, val_type, -1.0e300)
                : gcc_jit_context_new_rvalue_from_long(ctx, val_type, -0x7fffffffL - 1L);
            break;
        default:
            *out_ident = is_flt
                ? gcc_jit_context_new_rvalue_from_double(ctx, val_type, 0.0)
                : gcc_jit_context_zero(ctx, val_type);
            break;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Combines a thread-private reduction accumulator `local_val` into `shared_lval` under `GOMP_atomic_start`/`end`.
 * @details Uses `GOMP_atomic_start()` / `GOMP_atomic_end()` around the read-combine-store sequence so
 * integer, floating-point, logical, and `min`/`max` reductions are 100% race-free across all types.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] fn Active outlined `gcc_jit_function`.
 * @param[in] shared_lval Dereferenced shared destination lvalue (`*ctx->var_ptr`).
 * @param[in] local_val Thread-private reduction accumulator rvalue.
 * @param[in] val_type Lowered `gcc_jit_type *` of the reduction variable.
 * @param[in] red_op Reduction operator.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t emit_omp_reduction_combine(
    gcc_jit_context *ctx,
    gcc_jit_function *fn,
    gcc_jit_lvalue *shared_lval,
    gcc_jit_rvalue *local_val,
    gcc_jit_type *val_type,
    gcc_be_omp_red_op_t red_op) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_atomic_start"));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

    gcc_jit_rvalue *cur_shared = gcc_jit_lvalue_as_rvalue(shared_lval);
    if (red_op == GCC_BE_OMP_RED_MIN || red_op == GCC_BE_OMP_RED_MAX) {
        gcc_jit_block *upd_blk = gcc_jit_function_new_block(fn, "omp_red_minmax_upd");
        gcc_jit_block *cont_blk = gcc_jit_function_new_block(fn, "omp_red_minmax_cont");
        enum gcc_jit_comparison cmp = (red_op == GCC_BE_OMP_RED_MIN) ? GCC_JIT_COMPARISON_LT : GCC_JIT_COMPARISON_GT;
        gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, cmp, local_val, cur_shared);
        gcc_jit_block_end_with_conditional(cblock, NULL, cond, upd_blk, cont_blk);
        gcc_jit_block_add_assignment(upd_blk, NULL, shared_lval, local_val);
        gcc_jit_block_end_with_jump(upd_blk, NULL, cont_blk);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cont_blk));
    } else {
        enum gcc_jit_binary_op bop = GCC_JIT_BINARY_OP_PLUS;
        if (red_op == GCC_BE_OMP_RED_MUL) bop = GCC_JIT_BINARY_OP_MULT;
        else if (red_op == GCC_BE_OMP_RED_BITAND) bop = GCC_JIT_BINARY_OP_BITWISE_AND;
        else if (red_op == GCC_BE_OMP_RED_BITOR) bop = GCC_JIT_BINARY_OP_BITWISE_OR;
        else if (red_op == GCC_BE_OMP_RED_BITXOR) bop = GCC_JIT_BINARY_OP_BITWISE_XOR;
        else if (red_op == GCC_BE_OMP_RED_LOGAND) bop = GCC_JIT_BINARY_OP_LOGICAL_AND;
        else if (red_op == GCC_BE_OMP_RED_LOGOR) bop = GCC_JIT_BINARY_OP_LOGICAL_OR;

        gcc_jit_rvalue *combined = gcc_jit_context_new_binary_op(ctx, NULL, bop, val_type, cur_shared, local_val);
        gcc_jit_block_add_assignment(cblock, NULL, shared_lval, combined);
    }

    return emit_gomp_void_call("GOMP_atomic_end");
}

/**
 * @brief Extracts canonical OpenMP loop bounds `(iv, lb, ub, step, cmp_op)` from an EDG `stmk_for` statement.
 * @details Inspects `for_loop.extra_info->initialization` (`stmk_init` or `eok_assign`), `stmt->expr`
 * (`<`, `<=`, `>`, `>=`, `!=`), and `for_loop.extra_info->increment` (`++`, `--`, `+=`, `-=`, `= iv + step`).
 * @param[in] for_stmt Frontend `stmk_for` statement node.
 * @param[out] out_lvl Pointer receiving the extracted `gcc_be_omp_loop_level_t` descriptor.
 * @return `GCC_GEN_BE_SUCCESS` on valid canonical loop, or `GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t extract_canonical_omp_for_level(
    a_statement_ptr for_stmt,
    gcc_be_omp_loop_level_t *out_lvl) GCC_GEN_BE_NOEXCEPT {
    memset(out_lvl, 0, sizeof(*out_lvl));
    a_statement_ptr ext_init_s = NULL;
    while (for_stmt && for_stmt->kind == stmk_block && for_stmt->variant.block.statements) {
        a_statement_ptr s = for_stmt->variant.block.statements;
        while (s && (s->kind == stmk_decl || s->kind == stmk_empty)) {
            s = s->next;
        }
        if (s && s->kind == stmk_init && s->next && s->next->kind == stmk_for && s->next->next == NULL) {
            ext_init_s = s;
            for_stmt = s->next;
            break;
        }
        if (s && s->kind == stmk_for && s->next == NULL) {
            for_stmt = s;
            break;
        }
        break;
    }
    if (!for_stmt || for_stmt->kind != stmk_for || !for_stmt->variant.for_loop.extra_info || !for_stmt->expr) {
        return GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED;
    }

    a_statement_ptr init_s = for_stmt->variant.for_loop.extra_info->initialization;
    if (!init_s) {
        init_s = ext_init_s;
    }
    if (init_s && init_s->kind == stmk_block && init_s->variant.block.statements) {
        init_s = init_s->variant.block.statements;
        while (init_s && (init_s->kind == stmk_decl || init_s->kind == stmk_empty)) {
            init_s = init_s->next;
        }
    }
    if (init_s && init_s->kind == stmk_init && init_s->variant.dynamic_init) {
        a_dynamic_init_ptr dip = init_s->variant.dynamic_init;
        out_lvl->iv_var = dip->variable;
        if (dip->kind == dik_expression) {
            out_lvl->lb_expr = dip->variant.expression;
        } else if (dip->kind == dik_constant) {
            out_lvl->lb_const = dip->variant.constant.ptr;
        }
    } else if (init_s && init_s->kind == stmk_expr && init_s->expr &&
               init_s->expr->kind == enk_operation &&
               (init_s->expr->variant.operation.kind == eok_assign ||
                init_s->expr->variant.operation.kind == eok_bassign)) {
        an_expr_node_ptr lhs = init_s->expr->variant.operation.operands;
        if (lhs && lhs->kind == enk_variable) {
            out_lvl->iv_var = lhs->variant.variable.ptr;
            out_lvl->lb_expr = lhs->next;
        }
    }

    an_expr_node_ptr cond = for_stmt->expr;
    while (cond && cond->kind == enk_operation && cond->variant.operation.kind == eok_cast) {
        cond = cond->variant.operation.operands;
    }
    if (!out_lvl->iv_var || (!out_lvl->lb_expr && !out_lvl->lb_const) || !cond || cond->kind != enk_operation) {
        return GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED;
    }
    an_expr_operator_kind cop = cond->variant.operation.kind;
    an_expr_node_ptr c1 = cond->variant.operation.operands;
    an_expr_node_ptr c2 = c1 ? c1->next : NULL;
    if (!c1 || !c2) {
        return GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED;
    }
    out_lvl->cmp_op = cop;
    out_lvl->ub_expr = c2;

    out_lvl->step_sign = 1;
    an_expr_node_ptr inc = for_stmt->variant.for_loop.extra_info->increment;
    if (inc && inc->kind == enk_operation) {
        an_expr_operator_kind iop = inc->variant.operation.kind;
        an_expr_node_ptr iop1 = inc->variant.operation.operands;
        an_expr_node_ptr iop2 = iop1 ? iop1->next : NULL;
        if (iop == eok_pre_decr || iop == eok_post_decr) {
            out_lvl->step_sign = -1;
        } else if (iop == eok_add_assign) {
            out_lvl->step_expr = iop2;
        } else if (iop == eok_subtract_assign) {
            out_lvl->step_sign = -1;
            out_lvl->step_expr = iop2;
        } else if ((iop == eok_assign || iop == eok_bassign) && iop2 && iop2->kind == enk_operation) {
            if (iop2->variant.operation.kind == eok_add && iop2->variant.operation.operands) {
                out_lvl->step_expr = iop2->variant.operation.operands->next;
            } else if (iop2->variant.operation.kind == eok_subtract && iop2->variant.operation.operands) {
                out_lvl->step_sign = -1;
                out_lvl->step_expr = iop2->variant.operation.operands->next;
            }
        }
    }

    a_type_ptr iv_t = out_lvl->iv_var->type;
    while (iv_t && iv_t->kind == tk_typeref) iv_t = iv_t->variant.typeref.type;
    if (iv_t && iv_t->kind == tk_integer && iv_t->size >= 8) {
        an_integer_kind ik = iv_t->variant.integer.int_kind;
        if (ik == ik_unsigned_int || ik == ik_unsigned_long ||
            ik == ik_unsigned_long_long || ik == ik_unsigned_bit_precise) {
            out_lvl->is_unsigned_64 = TRUE;
        }
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(out_lvl->iv_var->type, &out_lvl->iv_jit_type));
    out_lvl->body_stmt = for_stmt->variant.for_loop.statement;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a single statement `s` without traversing any sibling `s->next` statements in its parent block.
 * @details Temporarily detaches `s->next`, invokes `gcc_gen_be_lower_statement(s, fn)`, and restores `s->next`.
 * @param[in] s Single statement node (may be NULL).
 * @param[in] fn Target `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t lower_single_omp_body_stmt(
    a_statement_ptr s,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    if (!s) return GCC_GEN_BE_SUCCESS;
    a_statement_ptr saved_next = s->next;
    s->next = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_lower_statement(s, fn);
    s->next = saved_next;
    return err;
}

/**
 * @brief Lowers a `#pragma omp for` (or inner loop of `#pragma omp parallel for`) via `GOMP_loop_*`.
 * @details Supports `static`, `dynamic`, `guided`, and `runtime` schedules, 32-bit and 64-bit
 * signed/unsigned bounds (`GOMP_loop_*` and `GOMP_loop_ull_*`), `collapse(K)`, `private`,
 * `firstprivate`, `lastprivate`, and `reduction`.
 * @param[in] dir Parsed OpenMP directive and clauses.
 * @param[in] for_stmt Outermost `stmk_for` statement node.
 * @param[in] func Enclosing `gcc_jit_function` (either parent function or outlined parallel callback).
 * @param[in] is_inside_parallel_for `TRUE` when `func` is already the dedicated `#pragma omp parallel for` callback.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t lower_omp_for_workshare(
    const gcc_be_omp_parsed_dir_t *dir,
    a_statement_ptr for_stmt,
    gcc_jit_function *func,
    a_boolean is_inside_parallel_for) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx || !cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    int K = dir->collapse_count;
    if (K > 8) K = 8;

    gcc_be_omp_loop_level_t levels[8];
    a_statement_ptr cur_s = for_stmt;
    for (int d = 0; d < K; ++d) {
        while (cur_s && cur_s->kind == stmk_block && cur_s->variant.block.statements &&
               !cur_s->variant.block.statements->next) {
            cur_s = cur_s->variant.block.statements;
        }
        GCC_GEN_BE_CHECK(extract_canonical_omp_for_level(cur_s, &levels[d]));
        cur_s = levels[d].body_stmt;
    }
    a_statement_ptr inner_body = levels[K - 1].body_stmt;

    a_boolean use_ull = levels[0].is_unsigned_64;
    gcc_jit_type *idx_t = gcc_jit_context_get_type(
        ctx, use_ull ? GCC_JIT_TYPE_UNSIGNED_LONG_LONG : GCC_JIT_TYPE_LONG);
    gcc_jit_type *idx_ptr_t = gcc_jit_type_get_pointer(idx_t);
    gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);

    /* Evaluate lb_d, step_d, and trip_count_d for each collapsed level */
    gcc_jit_lvalue *lb_slots[8];
    gcc_jit_lvalue *step_slots[8];
    gcc_jit_lvalue *trip_slots[8];
    void *saved_iv_bindings[8];
    gcc_jit_lvalue *outer_iv_lvals[8];

    gcc_jit_rvalue *total_trips = gcc_jit_context_one(ctx, idx_t);
    for (int d = 0; d < K; ++d) {
        void *prev_iv = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, levels[d].iv_var, &prev_iv));
        saved_iv_bindings[d] = prev_iv;
        outer_iv_lvals[d] = (gcc_jit_lvalue *)prev_iv;

        gcc_jit_rvalue *lb_rv = NULL;
        if (levels[d].lb_expr) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(levels[d].lb_expr, &lb_rv));
        } else {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(levels[d].lb_const, levels[d].iv_jit_type, &lb_rv));
        }
        gcc_jit_rvalue *ub_rv = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(levels[d].ub_expr, &ub_rv));
        gcc_jit_rvalue *st_rv = NULL;
        if (levels[d].step_expr) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(levels[d].step_expr, &st_rv));
        } else {
            st_rv = gcc_jit_context_one(ctx, idx_t);
        }

        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        gcc_jit_rvalue *lb_i = gcc_jit_context_new_cast(ctx, NULL, lb_rv, idx_t);
        gcc_jit_rvalue *ub_i = gcc_jit_context_new_cast(ctx, NULL, ub_rv, idx_t);
        gcc_jit_rvalue *abs_step = gcc_jit_context_new_cast(ctx, NULL, st_rv, idx_t);

        lb_slots[d] = gcc_jit_function_new_local(func, NULL, idx_t, "__omp_lb");
        step_slots[d] = gcc_jit_function_new_local(func, NULL, idx_t, "__omp_abs_step");
        trip_slots[d] = gcc_jit_function_new_local(func, NULL, idx_t, "__omp_trips");
        gcc_jit_block_add_assignment(cblock, NULL, lb_slots[d], lb_i);
        gcc_jit_block_add_assignment(cblock, NULL, step_slots[d], abs_step);

        /* Compute span and exact trip count for <, <=, >, >=, != */
        gcc_jit_rvalue *start_v = (levels[d].step_sign > 0) ? lb_i : ub_i;
        gcc_jit_rvalue *limit_v = (levels[d].step_sign > 0) ? ub_i : lb_i;
        a_boolean is_inclusive = (levels[d].cmp_op == eok_le || levels[d].cmp_op == eok_ge) ? TRUE : FALSE;

        gcc_jit_block *pos_blk = gcc_jit_function_new_block(func, "omp_trip_pos");
        gcc_jit_block *zero_blk = gcc_jit_function_new_block(func, "omp_trip_zero");
        gcc_jit_block *mrg_blk = gcc_jit_function_new_block(func, "omp_trip_merge");
        enum gcc_jit_comparison span_cmp = is_inclusive ? GCC_JIT_COMPARISON_LE : GCC_JIT_COMPARISON_LT;
        gcc_jit_rvalue *has_iters = gcc_jit_context_new_comparison(ctx, NULL, span_cmp, start_v, limit_v);
        gcc_jit_block_end_with_conditional(cblock, NULL, has_iters, pos_blk, zero_blk);

        gcc_jit_rvalue *diff = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, idx_t, limit_v, start_v);
        if (is_inclusive) {
            diff = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, idx_t, diff, abs_step);
        } else {
            gcc_jit_rvalue *step_m1 = gcc_jit_context_new_binary_op(
                ctx, NULL, GCC_JIT_BINARY_OP_MINUS, idx_t, abs_step, gcc_jit_context_one(ctx, idx_t));
            diff = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, idx_t, diff, step_m1);
        }
        gcc_jit_rvalue *tcnt = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, idx_t, diff, abs_step);
        gcc_jit_block_add_assignment(pos_blk, NULL, trip_slots[d], tcnt);
        gcc_jit_block_end_with_jump(pos_blk, NULL, mrg_blk);

        gcc_jit_block_add_assignment(zero_blk, NULL, trip_slots[d], gcc_jit_context_zero(ctx, idx_t));
        gcc_jit_block_end_with_jump(zero_blk, NULL, mrg_blk);
        cblock = mrg_blk;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cblock));

        total_trips = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_MULT, idx_t, total_trips, gcc_jit_lvalue_as_rvalue(trip_slots[d]));

        levels[d].iv_lval = gcc_jit_function_new_local(func, NULL, levels[d].iv_jit_type, "__omp_iv_priv");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, levels[d].iv_var, levels[d].iv_lval));
    }

    gcc_jit_lvalue *total_slot = gcc_jit_function_new_local(func, NULL, idx_t, "__omp_total_trips");
    gcc_jit_block_add_assignment(cblock, NULL, total_slot, total_trips);

    /* Handle private / firstprivate / lastprivate / reduction clauses on `#pragma omp for`
       (and `lastprivate` per-thread privatization inside `#pragma omp parallel for`) */
    gcc_be_omp_capture_t local_caps[32];
    void *saved_cap_bindings[32];
    int num_local_caps = 0;
    if (dir->num_clause_vars > 0) {
        a_variable_ptr body_vars[32];
        int n_bvars = 0;
        GCC_GEN_BE_CHECK(collect_vars_in_stmt(inner_body, body_vars, &n_bvars, 32));
        for (int vi = 0; vi < n_bvars; ++vi) {
            a_variable_ptr v = body_vars[vi];
            const char *vname = v->source_corresp.name;
            if (!vname) continue;
            for (int ci = 0; ci < dir->num_clause_vars; ++ci) {
                if (strcmp(dir->clause_vars[ci].name, vname) == 0 &&
                    dir->clause_vars[ci].sharing != GCC_BE_OMP_SHARE_SHARED &&
                    (!is_inside_parallel_for || dir->clause_vars[ci].sharing == GCC_BE_OMP_SHARE_LASTPRIVATE)) {
                    void *orig_lv = NULL;
                    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, v, &orig_lv));
                    int k = num_local_caps++;
                    local_caps[k].var = v;
                    local_caps[k].outer_lval = (gcc_jit_lvalue *)orig_lv;
                    saved_cap_bindings[k] = orig_lv;
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(v->type, &local_caps[k].val_type));
                    local_caps[k].sharing = dir->clause_vars[ci].sharing;
                    local_caps[k].red_op = dir->clause_vars[ci].red_op;
                    local_caps[k].inner_local = gcc_jit_function_new_local(
                        func, NULL, local_caps[k].val_type, "__omp_for_priv");
                    if (local_caps[k].sharing == GCC_BE_OMP_SHARE_FIRSTPRIVATE) {
                        gcc_jit_block_add_assignment(
                            cblock, NULL, local_caps[k].inner_local,
                            gcc_jit_lvalue_as_rvalue(local_caps[k].outer_lval));
                    } else if (local_caps[k].sharing == GCC_BE_OMP_SHARE_REDUCTION) {
                        gcc_jit_rvalue *id_rv = NULL;
                        GCC_GEN_BE_CHECK(make_omp_reduction_identity(
                            ctx, v->type, local_caps[k].val_type, local_caps[k].red_op, &id_rv));
                        gcc_jit_block_add_assignment(cblock, NULL, local_caps[k].inner_local, id_rv);
                    } else {
                        gcc_jit_rvalue *zr = NULL;
                        GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(v->type, local_caps[k].val_type, &zr));
                        gcc_jit_block_add_assignment(cblock, NULL, local_caps[k].inner_local, zr);
                    }
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, v, local_caps[k].inner_local));
                    break;
                }
            }
        }
    }

    /* Emit GOMP_loop_*_start / GOMP_loop_*_next / GOMP_loop_end */
    gcc_jit_lvalue *istart = gcc_jit_function_new_local(func, NULL, idx_t, "__gomp_istart");
    gcc_jit_lvalue *iend = gcc_jit_function_new_local(func, NULL, idx_t, "__gomp_iend");
    gcc_jit_lvalue *cur_idx = gcc_jit_function_new_local(func, NULL, idx_t, "__gomp_idx");

    const char *sched_tag = "static";
    if (dir->sched_kind == GCC_BE_OMP_SCHED_DYNAMIC) sched_tag = "dynamic";
    else if (dir->sched_kind == GCC_BE_OMP_SCHED_GUIDED) sched_tag = "guided";
    else if (dir->sched_kind == GCC_BE_OMP_SCHED_RUNTIME) sched_tag = "runtime";

    char start_sym[64];
    char next_sym[64];
    snprintf(start_sym, sizeof(start_sym), use_ull ? "GOMP_loop_ull_%s_start" : "GOMP_loop_%s_start", sched_tag);
    snprintf(next_sym, sizeof(next_sym), use_ull ? "GOMP_loop_ull_%s_next" : "GOMP_loop_%s_next", sched_tag);

    long chunk_v = dir->chunk_size;
    if (chunk_v <= 0 && (dir->sched_kind == GCC_BE_OMP_SCHED_DYNAMIC || dir->sched_kind == GCC_BE_OMP_SCHED_GUIDED)) {
        chunk_v = 1L;
    }

    gcc_jit_rvalue *has_chunk = NULL;
    if (dir->is_simd && !dir->is_for) {
        /* Pure `#pragma omp simd` sequential vectorizable loop [0, total_trips) */
        gcc_jit_block_add_assignment(cblock, NULL, istart, gcc_jit_context_zero(ctx, idx_t));
        gcc_jit_block_add_assignment(cblock, NULL, iend, gcc_jit_lvalue_as_rvalue(total_slot));
        has_chunk = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_LT, gcc_jit_context_zero(ctx, idx_t), gcc_jit_lvalue_as_rvalue(total_slot));
    } else if (dir->sched_kind == GCC_BE_OMP_SCHED_RUNTIME) {
        gcc_jit_function *start_fn = NULL;
        if (use_ull) {
            gcc_jit_type *pts[6] = { bool_t, idx_t, idx_t, idx_t, idx_ptr_t, idx_ptr_t };
            GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, start_sym, bool_t, 6, pts, &start_fn));
            gcc_jit_rvalue *sargs[6] = {
                gcc_jit_context_one(ctx, bool_t),
                gcc_jit_context_zero(ctx, idx_t),
                gcc_jit_lvalue_as_rvalue(total_slot),
                gcc_jit_context_one(ctx, idx_t),
                gcc_jit_lvalue_get_address(istart, NULL),
                gcc_jit_lvalue_get_address(iend, NULL)
            };
            has_chunk = gcc_jit_context_new_call(ctx, NULL, start_fn, 6, sargs);
        } else {
            gcc_jit_type *pts[5] = { idx_t, idx_t, idx_t, idx_ptr_t, idx_ptr_t };
            GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, start_sym, bool_t, 5, pts, &start_fn));
            gcc_jit_rvalue *sargs[5] = {
                gcc_jit_context_zero(ctx, idx_t),
                gcc_jit_lvalue_as_rvalue(total_slot),
                gcc_jit_context_one(ctx, idx_t),
                gcc_jit_lvalue_get_address(istart, NULL),
                gcc_jit_lvalue_get_address(iend, NULL)
            };
            has_chunk = gcc_jit_context_new_call(ctx, NULL, start_fn, 5, sargs);
        }
    } else {
        gcc_jit_function *start_fn = NULL;
        if (use_ull) {
            gcc_jit_type *pts[7] = { bool_t, idx_t, idx_t, idx_t, idx_t, idx_ptr_t, idx_ptr_t };
            GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, start_sym, bool_t, 7, pts, &start_fn));
            gcc_jit_rvalue *sargs[7] = {
                gcc_jit_context_one(ctx, bool_t),
                gcc_jit_context_zero(ctx, idx_t),
                gcc_jit_lvalue_as_rvalue(total_slot),
                gcc_jit_context_one(ctx, idx_t),
                gcc_jit_context_new_rvalue_from_long(ctx, idx_t, chunk_v),
                gcc_jit_lvalue_get_address(istart, NULL),
                gcc_jit_lvalue_get_address(iend, NULL)
            };
            has_chunk = gcc_jit_context_new_call(ctx, NULL, start_fn, 7, sargs);
        } else {
            gcc_jit_type *pts[6] = { idx_t, idx_t, idx_t, idx_t, idx_ptr_t, idx_ptr_t };
            GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, start_sym, bool_t, 6, pts, &start_fn));
            gcc_jit_rvalue *sargs[6] = {
                gcc_jit_context_zero(ctx, idx_t),
                gcc_jit_lvalue_as_rvalue(total_slot),
                gcc_jit_context_one(ctx, idx_t),
                gcc_jit_context_new_rvalue_from_long(ctx, idx_t, chunk_v),
                gcc_jit_lvalue_get_address(istart, NULL),
                gcc_jit_lvalue_get_address(iend, NULL)
            };
            has_chunk = gcc_jit_context_new_call(ctx, NULL, start_fn, 6, sargs);
        }
    }

    gcc_jit_block *chunk_hdr = gcc_jit_function_new_block(func, "omp_chunk_hdr");
    gcc_jit_block *iter_cond = gcc_jit_function_new_block(func, "omp_iter_cond");
    gcc_jit_block *iter_body = gcc_jit_function_new_block(func, "omp_iter_body");
    gcc_jit_block *iter_step = gcc_jit_function_new_block(func, "omp_iter_step");
    gcc_jit_block *chunk_next = gcc_jit_function_new_block(func, "omp_chunk_next");
    gcc_jit_block *loop_exit = gcc_jit_function_new_block(func, "omp_loop_exit");

    gcc_jit_block_end_with_conditional(cblock, NULL, has_chunk, chunk_hdr, loop_exit);

    gcc_jit_block_add_assignment(chunk_hdr, NULL, cur_idx, gcc_jit_lvalue_as_rvalue(istart));
    gcc_jit_block_end_with_jump(chunk_hdr, NULL, iter_cond);

    gcc_jit_rvalue *in_chunk = gcc_jit_context_new_comparison(
        ctx, NULL, GCC_JIT_COMPARISON_LT, gcc_jit_lvalue_as_rvalue(cur_idx), gcc_jit_lvalue_as_rvalue(iend));
    gcc_jit_block_end_with_conditional(iter_cond, NULL, in_chunk, iter_body, chunk_next);

    /* Reconstruct collapsed (iv_0, ..., iv_{K-1}) from linear index `cur_idx` */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(iter_body));
    gcc_jit_rvalue *rem = gcc_jit_lvalue_as_rvalue(cur_idx);
    for (int d = K - 1; d >= 0; --d) {
        gcc_jit_rvalue *td = gcc_jit_lvalue_as_rvalue(trip_slots[d]);
        gcc_jit_rvalue *coord = (d == 0)
            ? rem
            : gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MODULO, idx_t, rem, td);
        if (d > 0) {
            rem = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, idx_t, rem, td);
        }
        gcc_jit_rvalue *scaled = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_MULT, idx_t, coord, gcc_jit_lvalue_as_rvalue(step_slots[d]));
        enum gcc_jit_binary_op dir_op = (levels[d].step_sign > 0) ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS;
        gcc_jit_rvalue *iv_val = gcc_jit_context_new_binary_op(
            ctx, NULL, dir_op, idx_t, gcc_jit_lvalue_as_rvalue(lb_slots[d]), scaled);
        gcc_jit_block_add_assignment(
            iter_body, NULL, levels[d].iv_lval, gcc_jit_context_new_cast(ctx, NULL, iv_val, levels[d].iv_jit_type));
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_push_break_block(chunk_next));
    gcc_gen_be_error_t cont_err = gcc_gen_be_push_continue_block(iter_step);
    if (cont_err != GCC_GEN_BE_SUCCESS) {
        gcc_jit_block *pb = NULL;
        return gcc_gen_be_first_error(cont_err, gcc_gen_be_pop_break_block(&pb));
    }
    gcc_gen_be_error_t body_err = lower_single_omp_body_stmt(inner_body, func);
    gcc_jit_block *popped_b = NULL;
    gcc_jit_block *popped_c = NULL;
    body_err = gcc_gen_be_first_error(body_err, gcc_gen_be_pop_continue_block(&popped_c));
    body_err = gcc_gen_be_first_error(body_err, gcc_gen_be_pop_break_block(&popped_b));
    GCC_GEN_BE_CHECK(body_err);

    gcc_jit_block *after_body = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_body));
    if (after_body) {
        gcc_jit_block_end_with_jump(after_body, NULL, iter_step);
    }

    /* Update `lastprivate` variables on the final logical iteration (`cur_idx + 1 == total_trips`) */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(iter_step));
    gcc_jit_rvalue *next_idx = gcc_jit_context_new_binary_op(
        ctx, NULL, GCC_JIT_BINARY_OP_PLUS, idx_t, gcc_jit_lvalue_as_rvalue(cur_idx), gcc_jit_context_one(ctx, idx_t));
    a_boolean has_any_lastpriv = FALSE;
    for (int ci = 0; ci < dir->num_clause_vars; ++ci) {
        if (dir->clause_vars[ci].sharing == GCC_BE_OMP_SHARE_LASTPRIVATE) {
            has_any_lastpriv = TRUE;
            break;
        }
    }
    if (has_any_lastpriv) {
        gcc_jit_block *lp_store_blk = gcc_jit_function_new_block(func, "omp_lastpriv_store");
        gcc_jit_block *lp_after_blk = gcc_jit_function_new_block(func, "omp_lastpriv_after");
        gcc_jit_rvalue *is_last = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_EQ, next_idx, gcc_jit_lvalue_as_rvalue(total_slot));
        gcc_jit_block_end_with_conditional(iter_step, NULL, is_last, lp_store_blk, lp_after_blk);
        for (int k = 0; k < num_local_caps; ++k) {
            if (local_caps[k].sharing == GCC_BE_OMP_SHARE_LASTPRIVATE) {
                gcc_jit_block_add_assignment(
                    lp_store_blk, NULL, local_caps[k].outer_lval, gcc_jit_lvalue_as_rvalue(local_caps[k].inner_local));
            }
        }
        for (int d = 0; d < K; ++d) {
            const char *ivn = levels[d].iv_var->source_corresp.name;
            if (ivn && outer_iv_lvals[d]) {
                for (int ci = 0; ci < dir->num_clause_vars; ++ci) {
                    if (dir->clause_vars[ci].sharing == GCC_BE_OMP_SHARE_LASTPRIVATE &&
                        strcmp(dir->clause_vars[ci].name, ivn) == 0) {
                        gcc_jit_block_add_assignment(
                            lp_store_blk, NULL, outer_iv_lvals[d], gcc_jit_lvalue_as_rvalue(levels[d].iv_lval));
                    }
                }
            }
        }
        gcc_jit_block_end_with_jump(lp_store_blk, NULL, lp_after_blk);
        iter_step = lp_after_blk;
    }
    gcc_jit_block_add_assignment(iter_step, NULL, cur_idx, next_idx);
    gcc_jit_block_end_with_jump(iter_step, NULL, iter_cond);

    /* Request next work-sharing chunk */
    if (dir->is_simd && !dir->is_for) {
        gcc_jit_block_end_with_jump(chunk_next, NULL, loop_exit);
    } else {
        gcc_jit_function *next_fn = NULL;
        gcc_jit_type *npts[2] = { idx_ptr_t, idx_ptr_t };
        GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, next_sym, bool_t, 2, npts, &next_fn));
        gcc_jit_rvalue *nargs[2] = {
            gcc_jit_lvalue_get_address(istart, NULL),
            gcc_jit_lvalue_get_address(iend, NULL)
        };
        gcc_jit_rvalue *more = gcc_jit_context_new_call(ctx, NULL, next_fn, 2, nargs);
        gcc_jit_block_end_with_conditional(chunk_next, NULL, more, chunk_hdr, loop_exit);
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(loop_exit));
    if (dir->is_for) {
        const char *end_sym = (dir->has_nowait || is_inside_parallel_for) ? "GOMP_loop_end_nowait" : "GOMP_loop_end";
        GCC_GEN_BE_CHECK(emit_gomp_void_call(end_sym));
    }

    /* Combine standalone `#pragma omp for` reductions and restore outer variable bindings */
    for (int k = 0; k < num_local_caps; ++k) {
        if (local_caps[k].sharing == GCC_BE_OMP_SHARE_REDUCTION) {
            GCC_GEN_BE_CHECK(emit_omp_reduction_combine(
                ctx, func, local_caps[k].outer_lval,
                gcc_jit_lvalue_as_rvalue(local_caps[k].inner_local),
                local_caps[k].val_type, local_caps[k].red_op));
        }
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, local_caps[k].var, saved_cap_bindings[k]));
    }
    for (int d = 0; d < K; ++d) {
        if (saved_iv_bindings[d]) {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, levels[d].iv_var, saved_iv_bindings[d]));
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Outlines an OpenMP `parallel`, `parallel for`, or `task` region into an internal callback function.
 * @details Packs captured outer variables into a synthesized struct `__edg_omp_ctx_N`, emits the
 * internal callback `void __edg_omp_fn_N(void *__omp_data)`, lowers `stmt` (or `lower_omp_for_workshare`
 * for `parallel for`), combines reductions atomically, restores all parent function compiler state,
 * and emits `GOMP_parallel` or `GOMP_task` in the caller block.
 * @param[in] dir Parsed OpenMP directive and clauses.
 * @param[in] stmt Body statement (`stmk_block`, `stmk_for`, etc.).
 * @param[in] parent_func Enclosing `gcc_jit_function` (`unused` other than allocating the context struct).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t lower_omp_outlined_region(
    const gcc_be_omp_parsed_dir_t *dir,
    a_statement_ptr stmt,
    gcc_jit_function *parent_func) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *caller_block = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&caller_block));
    if (!ctx || !caller_block) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    a_variable_ptr ref_vars[48];
    int num_ref_vars = 0;
    if (stmt->kind == stmk_block) {
        GCC_GEN_BE_CHECK(collect_vars_in_stmt_list(stmt->variant.block.statements, ref_vars, &num_ref_vars, 48));
    } else {
        GCC_GEN_BE_CHECK(collect_vars_in_stmt(stmt, ref_vars, &num_ref_vars, 48));
    }

    /* Exclude loop induction variables of collapsed `parallel for` unless `lastprivate` */
    a_variable_ptr loop_ivs[8];
    int num_loop_ivs = 0;
    if (dir->is_for) {
        a_statement_ptr cs = stmt;
        int max_collapse = (dir->collapse_count > 8) ? 8 : dir->collapse_count;
        for (int d = 0; d < max_collapse; ++d) {
            gcc_be_omp_loop_level_t tmp_lvl;
            if (extract_canonical_omp_for_level(cs, &tmp_lvl) != GCC_GEN_BE_SUCCESS) break;
            loop_ivs[num_loop_ivs++] = tmp_lvl.iv_var;
            cs = tmp_lvl.body_stmt;
        }
    }

    gcc_be_omp_capture_t caps[48];
    gcc_jit_field *ctx_fields[48];
    int num_caps = 0;
    int num_ctx_fields = 0;

    for (int i = 0; i < num_ref_vars; ++i) {
        a_variable_ptr v = ref_vars[i];
        const char *vname = v->source_corresp.name ? v->source_corresp.name : "anon";

        gcc_be_omp_var_sharing_t sharing = dir->is_task
            ? GCC_BE_OMP_SHARE_FIRSTPRIVATE
            : GCC_BE_OMP_SHARE_SHARED;
        gcc_be_omp_red_op_t red_op = GCC_BE_OMP_RED_NONE;

        a_boolean is_iv = FALSE;
        for (int d = 0; d < num_loop_ivs; ++d) {
            if (loop_ivs[d] == v) {
                is_iv = TRUE;
                sharing = GCC_BE_OMP_SHARE_PRIVATE;
                break;
            }
        }
        for (int ci = 0; ci < dir->num_clause_vars; ++ci) {
            if (strcmp(dir->clause_vars[ci].name, vname) == 0) {
                sharing = dir->clause_vars[ci].sharing;
                red_op = dir->clause_vars[ci].red_op;
                break;
            }
        }
        if (is_iv && sharing == GCC_BE_OMP_SHARE_PRIVATE) {
            continue;
        }

        void *orig_lv = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, v, &orig_lv));
        gcc_jit_type *vt = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(v->type, &vt));

        int k = num_caps++;
        caps[k].var = v;
        caps[k].outer_lval = (gcc_jit_lvalue *)orig_lv;
        caps[k].val_type = vt;
        caps[k].sharing = sharing;
        caps[k].red_op = red_op;
        caps[k].ctx_field = NULL;
        caps[k].inner_local = NULL;

        if (sharing != GCC_BE_OMP_SHARE_PRIVATE) {
            char fname[80];
            snprintf(fname, sizeof(fname), "f_%d_%s", k, vname);
            gcc_jit_type *ft = (sharing == GCC_BE_OMP_SHARE_FIRSTPRIVATE) ? vt : gcc_jit_type_get_pointer(vt);
            caps[k].ctx_field = gcc_jit_context_new_field(ctx, NULL, ft, fname);
            ctx_fields[num_ctx_fields++] = caps[k].ctx_field;
        }
    }

    unsigned long out_id = ++gcc_gen_be_singleton_state.omp_outlined_counter;
    if (num_ctx_fields == 0) {
        ctx_fields[0] = gcc_jit_context_new_field(
            ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "__dummy");
        num_ctx_fields = 1;
    }
    char ctx_type_name[64];
    snprintf(ctx_type_name, sizeof(ctx_type_name), "__edg_omp_ctx_%lu", out_id);
    gcc_jit_struct *ctx_struct = gcc_jit_context_new_struct_type(ctx, NULL, ctx_type_name, num_ctx_fields, ctx_fields);
    gcc_jit_type *ctx_type = gcc_jit_struct_as_type(ctx_struct);
    gcc_jit_type *ctx_ptr_type = gcc_jit_type_get_pointer(ctx_type);
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    /* Populate context struct in caller block prior to GOMP_parallel / GOMP_task */
    gcc_jit_lvalue *ctx_local = gcc_jit_function_new_local(parent_func, NULL, ctx_type, "__omp_ctx_arg");
    for (int k = 0; k < num_caps; ++k) {
        if (!caps[k].ctx_field) continue;
        gcc_jit_lvalue *fld_lv = gcc_jit_lvalue_access_field(ctx_local, NULL, caps[k].ctx_field);
        if (caps[k].sharing == GCC_BE_OMP_SHARE_FIRSTPRIVATE) {
            gcc_jit_block_add_assignment(caller_block, NULL, fld_lv, gcc_jit_lvalue_as_rvalue(caps[k].outer_lval));
        } else {
            gcc_jit_block_add_assignment(caller_block, NULL, fld_lv, gcc_jit_lvalue_get_address(caps[k].outer_lval, NULL));
        }
    }

    /* Create internal outlined callback function `void __edg_omp_fn_N(void *__omp_data)` */
    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__edg_omp_fn_%lu", out_id);
    gcc_jit_param *data_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_t, "__omp_data");
    gcc_jit_function *outlined_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, fn_name, 1, &data_param, 0);
    gcc_jit_block *out_entry = gcc_jit_function_new_block(outlined_fn, "omp_entry");
    gcc_jit_rvalue *typed_ctx_ptr = gcc_jit_context_new_cast(
        ctx, NULL, gcc_jit_param_as_rvalue(data_param), ctx_ptr_type);
    gcc_jit_lvalue *ctx_deref = gcc_jit_rvalue_dereference(typed_ctx_ptr, NULL);

    /* Save parent function's scope/loop stacks and rebind captured variables in GCC_GEN_BE_CACHE_VAR */
    gcc_gen_be_context_t *be_state = &gcc_gen_be_singleton_state;
    struct gcc_gen_be_scope **saved_scope_stack = be_state->scope_stack;
    size_t saved_scope_size = be_state->scope_stack_size;
    size_t saved_scope_cap = be_state->scope_stack_capacity;
    be_state->scope_stack = NULL;
    be_state->scope_stack_size = 0;
    be_state->scope_stack_capacity = 0;

    for (int k = 0; k < num_caps; ++k) {
        if (caps[k].sharing == GCC_BE_OMP_SHARE_SHARED ||
            caps[k].sharing == GCC_BE_OMP_SHARE_LASTPRIVATE) {
            gcc_jit_rvalue *p_rv = gcc_jit_rvalue_access_field(
                gcc_jit_lvalue_as_rvalue(ctx_deref), NULL, caps[k].ctx_field);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, caps[k].var, gcc_jit_rvalue_dereference(p_rv, NULL)));
        } else {
            caps[k].inner_local = gcc_jit_function_new_local(outlined_fn, NULL, caps[k].val_type, "__omp_priv");
            if (caps[k].sharing == GCC_BE_OMP_SHARE_FIRSTPRIVATE) {
                gcc_jit_rvalue *init_v = gcc_jit_rvalue_access_field(
                    gcc_jit_lvalue_as_rvalue(ctx_deref), NULL, caps[k].ctx_field);
                gcc_jit_block_add_assignment(out_entry, NULL, caps[k].inner_local, init_v);
            } else if (caps[k].sharing == GCC_BE_OMP_SHARE_REDUCTION) {
                gcc_jit_rvalue *id_rv = NULL;
                GCC_GEN_BE_CHECK(make_omp_reduction_identity(
                    ctx, caps[k].var->type, caps[k].val_type, caps[k].red_op, &id_rv));
                gcc_jit_block_add_assignment(out_entry, NULL, caps[k].inner_local, id_rv);
            } else {
                gcc_jit_rvalue *zr = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(caps[k].var->type, caps[k].val_type, &zr));
                gcc_jit_block_add_assignment(out_entry, NULL, caps[k].inner_local, zr);
            }
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, caps[k].var, caps[k].inner_local));
        }
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(out_entry));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));

    gcc_gen_be_error_t sub_err = dir->is_for
        ? lower_omp_for_workshare(dir, stmt, outlined_fn, TRUE)
        : lower_single_omp_body_stmt(stmt, outlined_fn);

    gcc_jit_block *out_tail = NULL;
    gcc_gen_be_error_t tail_err = gcc_gen_be_get_current_block(&out_tail);
    if (sub_err == GCC_GEN_BE_SUCCESS && tail_err == GCC_GEN_BE_SUCCESS && out_tail) {
        for (int k = 0; k < num_caps && sub_err == GCC_GEN_BE_SUCCESS; ++k) {
            if (caps[k].sharing == GCC_BE_OMP_SHARE_REDUCTION) {
                gcc_jit_rvalue *p_rv = gcc_jit_rvalue_access_field(
                    gcc_jit_lvalue_as_rvalue(ctx_deref), NULL, caps[k].ctx_field);
                gcc_jit_lvalue *shared_lv = gcc_jit_rvalue_dereference(p_rv, NULL);
                sub_err = emit_omp_reduction_combine(
                    ctx, outlined_fn, shared_lv,
                    gcc_jit_lvalue_as_rvalue(caps[k].inner_local),
                    caps[k].val_type, caps[k].red_op);
            }
        }
        if (sub_err == GCC_GEN_BE_SUCCESS) {
            sub_err = gcc_gen_be_get_current_block(&out_tail);
        }
    } else if (sub_err == GCC_GEN_BE_SUCCESS) {
        sub_err = tail_err;
    }
    gcc_gen_be_error_t pop_err = gcc_gen_be_pop_scope(out_tail);
    if (sub_err == GCC_GEN_BE_SUCCESS && pop_err != GCC_GEN_BE_SUCCESS) {
        sub_err = pop_err;
    }
    if (sub_err == GCC_GEN_BE_SUCCESS && out_tail) {
        gcc_jit_block_end_with_void_return(out_tail, NULL);
    }

    /* Restore parent scope stack, variable bindings, and active caller block */
    free(be_state->scope_stack);
    be_state->scope_stack = saved_scope_stack;
    be_state->scope_stack_size = saved_scope_size;
    be_state->scope_stack_capacity = saved_scope_cap;
    for (int k = 0; k < num_caps; ++k) {
        gcc_gen_be_error_t r_err = cache_insert(GCC_GEN_BE_CACHE_VAR, caps[k].var, caps[k].outer_lval);
        if (sub_err == GCC_GEN_BE_SUCCESS && r_err != GCC_GEN_BE_SUCCESS) {
            sub_err = r_err;
        }
    }
    gcc_gen_be_error_t cb_err = gcc_gen_be_set_current_block(caller_block);
    if (sub_err == GCC_GEN_BE_SUCCESS && cb_err != GCC_GEN_BE_SUCCESS) {
        sub_err = cb_err;
    }
    GCC_GEN_BE_CHECK(sub_err);

    gcc_jit_type *cb_fn_ptr_t = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_t, 1, &void_ptr_t, 0);
    gcc_jit_rvalue *fn_ptr_rv = gcc_jit_function_get_address(outlined_fn, NULL);
    gcc_jit_rvalue *ctx_void_ptr = gcc_jit_context_new_cast(
        ctx, NULL, gcc_jit_lvalue_get_address(ctx_local, NULL), void_ptr_t);

    if (dir->is_task) {
        /* GOMP_task(fn, data, cpyfn, arg_size, arg_align, if_clause, flags, depend, priority, detach) */
        gcc_jit_type *long_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
        gcc_jit_type *uint_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
        gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_type *cpy_pts[2] = { void_ptr_t, void_ptr_t };
        gcc_jit_type *cpy_fn_t = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_t, 2, cpy_pts, 0);
        gcc_jit_type *vpp_t = gcc_jit_type_get_pointer(void_ptr_t);

        gcc_jit_type *t_pts[10] = {
            cb_fn_ptr_t, void_ptr_t, cpy_fn_t, long_t, long_t, bool_t, uint_t, vpp_t, int_t, void_ptr_t
        };
        gcc_jit_function *task_fn = NULL;
        GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, "GOMP_task", void_t, 10, t_pts, &task_fn));
        long ctx_bytes = (long)(num_ctx_fields * 16);
        gcc_jit_rvalue *t_args[10] = {
            fn_ptr_rv,
            ctx_void_ptr,
            gcc_jit_context_null(ctx, cpy_fn_t),
            gcc_jit_context_new_rvalue_from_long(ctx, long_t, ctx_bytes),
            gcc_jit_context_new_rvalue_from_long(ctx, long_t, (long)sizeof(void *)),
            dir->if_is_false ? gcc_jit_context_zero(ctx, bool_t) : gcc_jit_context_one(ctx, bool_t),
            gcc_jit_context_zero(ctx, uint_t),
            gcc_jit_context_null(ctx, vpp_t),
            gcc_jit_context_zero(ctx, int_t),
            gcc_jit_context_null(ctx, void_ptr_t)
        };
        gcc_jit_block_add_eval(caller_block, NULL, gcc_jit_context_new_call(ctx, NULL, task_fn, 10, t_args));
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *uint_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
    gcc_jit_type *p_pts[4] = { cb_fn_ptr_t, void_ptr_t, uint_t, uint_t };
    gcc_jit_function *par_fn = NULL;
    GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, "GOMP_parallel", void_t, 4, p_pts, &par_fn));
    unsigned int nthreads = dir->if_is_false ? 1U : dir->num_threads;
    gcc_jit_rvalue *p_args[4] = {
        fn_ptr_rv,
        ctx_void_ptr,
        gcc_jit_context_new_rvalue_from_int(ctx, uint_t, (int)nthreads),
        gcc_jit_context_zero(ctx, uint_t)
    };
    gcc_jit_block_add_eval(caller_block, NULL, gcc_jit_context_new_call(ctx, NULL, par_fn, 4, p_args));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers `#pragma omp critical` / `critical(name)`, `single`, `master`/`masked`, and `atomic`.
 * @details Emits structured `libgomp` synchronization calls around `stmt` inside `func`.
 * @param[in] dir Parsed OpenMP directive descriptor.
 * @param[in] stmt Associated structured statement.
 * @param[in] func Enclosing `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t lower_omp_sync_construct(
    const gcc_be_omp_parsed_dir_t *dir,
    a_statement_ptr stmt,
    gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx || !cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    if (dir->is_critical) {
        if (dir->critical_name[0] != '\0') {
            char lock_sym[96];
            snprintf(lock_sym, sizeof(lock_sym), "__edg_gomp_crit_%s", dir->critical_name);
            void *cached_lock = NULL;
            gcc_jit_lvalue *lock_glob = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)lock_sym, &cached_lock));
            if (cached_lock) {
                lock_glob = (gcc_jit_lvalue *)cached_lock;
            } else {
                lock_glob = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, void_ptr_t, lock_sym);
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)lock_sym, lock_glob));
            }
            gcc_jit_type *vpp_t = gcc_jit_type_get_pointer(void_ptr_t);
            gcc_jit_function *cs_fn = NULL;
            gcc_jit_function *ce_fn = NULL;
            GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, "GOMP_critical_name_start", void_t, 1, &vpp_t, &cs_fn));
            GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, "GOMP_critical_name_end", void_t, 1, &vpp_t, &ce_fn));
            gcc_jit_rvalue *lock_addr = gcc_jit_lvalue_get_address(lock_glob, NULL);
            gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, cs_fn, 1, &lock_addr));
            GCC_GEN_BE_CHECK(lower_single_omp_body_stmt(stmt, func));
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
            if (cblock) {
                gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, ce_fn, 1, &lock_addr));
            }
            return GCC_GEN_BE_SUCCESS;
        }
        GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_critical_start"));
        GCC_GEN_BE_CHECK(lower_single_omp_body_stmt(stmt, func));
        return emit_gomp_void_call("GOMP_critical_end");
    }

    if (dir->is_single) {
        gcc_jit_function *sg_fn = NULL;
        GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, "GOMP_single_start", bool_t, 0, NULL, &sg_fn));
        gcc_jit_rvalue *won = gcc_jit_context_new_call(ctx, NULL, sg_fn, 0, NULL);
        gcc_jit_block *body_blk = gcc_jit_function_new_block(func, "omp_single_body");
        gcc_jit_block *after_blk = gcc_jit_function_new_block(func, "omp_single_after");
        gcc_jit_block_end_with_conditional(cblock, NULL, won, body_blk, after_blk);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_blk));
        GCC_GEN_BE_CHECK(lower_single_omp_body_stmt(stmt, func));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&body_blk));
        if (body_blk) {
            gcc_jit_block_end_with_jump(body_blk, NULL, after_blk);
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(after_blk));
        if (!dir->has_nowait) {
            GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_barrier"));
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (dir->is_master_or_masked) {
        gcc_jit_function *tid_fn = NULL;
        GCC_GEN_BE_CHECK(get_gomp_runtime_fn(ctx, "omp_get_thread_num", int_t, 0, NULL, &tid_fn));
        gcc_jit_rvalue *tid = gcc_jit_context_new_call(ctx, NULL, tid_fn, 0, NULL);
        gcc_jit_rvalue *want = gcc_jit_context_new_rvalue_from_int(ctx, int_t, dir->filter_tid);
        gcc_jit_rvalue *is_tid = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, tid, want);
        gcc_jit_block *body_blk = gcc_jit_function_new_block(func, "omp_master_body");
        gcc_jit_block *after_blk = gcc_jit_function_new_block(func, "omp_master_after");
        gcc_jit_block_end_with_conditional(cblock, NULL, is_tid, body_blk, after_blk);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_blk));
        GCC_GEN_BE_CHECK(lower_single_omp_body_stmt(stmt, func));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&body_blk));
        if (body_blk) {
            gcc_jit_block_end_with_jump(body_blk, NULL, after_blk);
        }
        return gcc_gen_be_set_current_block(after_blk);
    }

    /* `#pragma omp atomic` (`read`, `write`, `update`, `capture`) */
    if (stmt->kind == stmk_expr && stmt->expr && stmt->expr->kind == enk_operation) {
        an_expr_operator_kind op = stmt->expr->variant.operation.kind;
        an_expr_node_ptr a1 = stmt->expr->variant.operation.operands;
        an_expr_node_ptr a2 = a1 ? a1->next : NULL;
        if (dir->atomic_mode == 0 && a1 &&
            (op == eok_add_assign || op == eok_subtract_assign || op == eok_and_assign ||
             op == eok_or_assign || op == eok_xor_assign || op == eok_pre_incr ||
             op == eok_post_incr || op == eok_pre_decr || op == eok_post_decr)) {
            a_type_ptr ut = a1->type;
            while (ut && ut->kind == tk_typeref) ut = ut->variant.typeref.type;
            unsigned long sz = ut ? (unsigned long)ut->size : 0UL;
            if (sz == 1UL || sz == 2UL || sz == 4UL || sz == 8UL) {
                gcc_jit_lvalue *lv = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(a1, &lv));
                gcc_jit_rvalue *rv = NULL;
                if (a2) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(a2, &rv));
                }
                gcc_jit_rvalue *unused_res = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(op, a1->type, lv, rv, &unused_res));
                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                gcc_jit_block_add_eval(cblock, NULL, unused_res);
                return GCC_GEN_BE_SUCCESS;
            }
        } else if (dir->atomic_mode == 2 && a1 && a2 && (op == eok_assign || op == eok_bassign)) {
            a_type_ptr ut = a1->type;
            while (ut && ut->kind == tk_typeref) ut = ut->variant.typeref.type;
            unsigned long sz = ut ? (unsigned long)ut->size : 0UL;
            if (sz == 1UL || sz == 2UL || sz == 4UL || sz == 8UL) {
                gcc_jit_lvalue *lv = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(a1, &lv));
                gcc_jit_rvalue *rv = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(a2, &rv));
                gcc_jit_rvalue *unused_res = NULL;
                return gcc_gen_be_lower_c11_atomic_store(a1->type, lv, rv, &unused_res);
            }
        }
    }
    GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_atomic_start"));
    GCC_GEN_BE_CHECK(lower_single_omp_body_stmt(stmt, func));
    return emit_gomp_void_call("GOMP_atomic_end");
}

gcc_gen_be_error_t gcc_gen_be_try_lower_omp_statement(
    a_statement_ptr stmt,
    struct gcc_jit_function *func,
    a_boolean *out_handled) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func || !out_handled) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_handled = FALSE;

    gcc_gen_be_context_t *state = &gcc_gen_be_singleton_state;
    if (state->omp_pragma_count == 0) {
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_be_omp_pragma_entry_t *arr = (gcc_be_omp_pragma_entry_t *)state->omp_pragma_entries;
    for (size_t i = 0; i < state->omp_pragma_count; ++i) {
        if (arr[i].consumed || arr[i].stmt != stmt) {
            continue;
        }
        arr[i].consumed = TRUE;
        gcc_be_omp_parsed_dir_t dir;
        GCC_GEN_BE_CHECK(parse_omp_directive(arr[i].pragma_text, &dir));

        if (dir.is_barrier) {
            GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_barrier"));
            if (stmt->kind == stmk_empty) {
                *out_handled = TRUE;
                return GCC_GEN_BE_SUCCESS;
            }
            continue;
        }
        if (dir.is_taskwait) {
            GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_taskwait"));
            if (stmt->kind == stmk_empty) {
                *out_handled = TRUE;
                return GCC_GEN_BE_SUCCESS;
            }
            continue;
        }

        *out_handled = TRUE;
        if (dir.is_parallel || dir.is_task) {
            return lower_omp_outlined_region(&dir, stmt, func);
        }
        if (dir.is_for || dir.is_simd) {
            return lower_omp_for_workshare(&dir, stmt, func, FALSE);
        }
        return lower_omp_sync_construct(&dir, stmt, func);
    }

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_flush_trailing_omp_pragmas(
    a_scope_ptr scope,
    struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    if (!scope || gcc_gen_be_singleton_state.omp_pragma_count == 0) {
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_be_omp_pragma_entry_t *arr = (gcc_be_omp_pragma_entry_t *)gcc_gen_be_singleton_state.omp_pragma_entries;
    for (size_t i = 0; i < gcc_gen_be_singleton_state.omp_pragma_count; ++i) {
        if (arr[i].consumed || arr[i].stmt != NULL || arr[i].owner_scope != scope) {
            continue;
        }
        arr[i].consumed = TRUE;
        gcc_be_omp_parsed_dir_t dir;
        GCC_GEN_BE_CHECK(parse_omp_directive(arr[i].pragma_text, &dir));
        if (dir.is_barrier) {
            GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_barrier"));
        } else if (dir.is_taskwait) {
            GCC_GEN_BE_CHECK(emit_gomp_void_call("GOMP_taskwait"));
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
