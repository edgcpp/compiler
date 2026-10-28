/**
 * @file gcc_gen_be_eh.h
 * @brief Exception handling and stack unwinding for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_EH_H
#define GCC_GEN_BE_EH_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Ensures that all necessary EH symbols (e.g. __cxa_throw) are declared.
 *
 * @param ctx The libgccjit context.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_declare_eh_symbols(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Initialize exception handling state for a new function.
 *
 * @param func The JIT function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_begin_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Finish exception handling generation for a function (emits LSDA tables).
 *
 * @param func The JIT function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_end_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a throw of an exception.
 *
 * @param expr The throw expression node.
 * @param out_block Pointer to receive the updated basic block or new blocks.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_throw(an_expr_node_ptr expr, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a try-catch block using SjLj fallback.
 *
 * @param stmt The try-catch statement node.
 * @param func The JIT function being generated.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_try_block(a_statement_ptr stmt, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;


/**
 * @brief Lowers a function body, synthesizing exception specification enforcement if needed.
 *
 * @param stmt The function body statement.
 * @param func The JIT function being generated.
 * @param exc_spec The exception specification, or NULL.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_routine_body(a_statement_ptr stmt, gcc_jit_function *func, an_exception_specification_ptr exc_spec) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits an automatic DWARF cleanup-only landing pad (`action_table_index = 0`) for an RAII scope.
 * @details Records a call-site region in the LSDA table with `action_table_index = 0`, invokes all active
 * scope destructors in reverse construction order, and calls `_Unwind_Resume`.
 * @param[in] func The JIT function containing the scope.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits an `_Unwind_Resume` (or `__builtin_unwind_resume`) call at the end of a cleanup landing pad.
 * @details Terminates the current basic block by invoking `_Unwind_Resume(exc_ptr)` and jumping to a newly
 * created post-unwind continuation block returned in `out_terminated_block`.
 * @param[in] exc_ptr The unwinder exception header pointer rvalue.
 * @param[out] out_terminated_block Pointer to receive the new continuation block after the terminated landing pad.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on invalid arguments or state.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_unwind_resume(
    gcc_jit_rvalue *exc_ptr,
    gcc_jit_block **out_terminated_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Registers and attaches the target ABI personality routine to a function containing EH regions.
 * @details Declares or retrieves `__gxx_personality_v0` (or `__gxx_personality_seh0` / `__gcc_personality_v0`
 * per target ABI and dialect) and associates it with `func`.
 * @param[in] func The JIT function containing exception handling regions.
 * @param[out] out_personality_fn Pointer to receive the declared personality `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on invalid arguments or state.
 */
extern gcc_gen_be_error_t gcc_gen_be_register_personality_routine(
    gcc_jit_function *func,
    gcc_jit_function **out_personality_fn) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Synthesizes `.gcc_except_table` LSDA call-site, action, and type-info table entries for a scope.
 * @details Emits the LSDA metadata corresponding to active try blocks, catch clauses (`a_handler_ptr`),
 * and exception specifications, returning the global descriptor lvalue in `out_lsda_global`.
 * @param[in] scope The EDG scope pointer (may be NULL to use the active function's accumulated EH regions).
 * @param[out] out_lsda_global Pointer to receive the synthesized `.gcc_except_table` global lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_lsda_call_site_table(
    a_scope_ptr scope,
    struct gcc_jit_lvalue **out_lsda_global) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits Windows x64 / ARM64 `.pdata` and `.xdata` SEH unwind descriptor tables.
 * @details Synthesizes `UNWIND_INFO` (`UNW_FLAG_EHANDLER` / `UNW_FLAG_UHANDLER`) and
 * `__C_specific_handler` scope table entries in `.xdata` and `RUNTIME_FUNCTION` descriptors in `.pdata`.
 * @param[in] func The `gcc_jit_function` owning the SEH scopes.
 * @param[in] has_except_handler `TRUE` if the scope uses `__except` (`UNW_FLAG_EHANDLER`), `FALSE` for `__finally` (`UNW_FLAG_UHANDLER`).
 * @param[out] out_xdata_global Optional pointer to receive the synthesized `.xdata` global lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_seh_unwind_tables(
    struct gcc_jit_function *func,
    a_boolean has_except_handler,
    struct gcc_jit_lvalue **out_xdata_global) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Enforces C++11 `noexcept(true)` or legacy dynamic `throw(T1, T2, ...)` specifications on a landing pad.
 * @details For `noexcept(true)`, emits a call to `std::terminate` (`_ZSt9terminatev`). For dynamic exception
 * specifications, matches the thrown exception against each allowed type in the filter list and calls
 * `__cxa_call_unexpected` if none match.
 * @param[in] func The JIT function being lowered.
 * @param[in] exc_spec The EDG exception specification descriptor.
 * @param[in] lp_block The landing pad basic block where exception enforcement begins.
 * @param[in] end_block The continuation block to jump to after enforcement.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_enforce_exception_spec(
    gcc_jit_function *func,
    an_exception_specification_ptr exc_spec,
    gcc_jit_block *lp_block,
    gcc_jit_block *end_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a C++11 `noexcept(true)` termination filter on a landing pad block.
 * @details Captures the active unwind exception registers, runs pending scope cleanups,
 * invokes `_ZSt9terminatev` (`std::terminate`), and jumps to `end_block`.
 * @param[in] func The JIT function being lowered.
 * @param[in] lp_block The landing pad basic block where `noexcept` termination is enforced.
 * @param[in] end_block The continuation block to jump to after `_ZSt9terminatev`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_noexcept_filter(
    gcc_jit_function *func,
    gcc_jit_block *lp_block,
    gcc_jit_block *end_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Encodes an unsigned integer value into DWARF ULEB128 byte format.
 * @details Writes the variable-length ULEB128 representation of `value` into `out_buf`
 * (up to `buf_cap` bytes) and writes the number of bytes written to `out_len`.
 * @param[in] value The unsigned 64-bit value to encode.
 * @param[out] out_buf Destination buffer for encoded bytes.
 * @param[in] buf_cap Capacity of `out_buf` in bytes.
 * @param[out] out_len Pointer to receive the number of encoded bytes written.
 * @return GCC_GEN_BE_SUCCESS on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if pointers are NULL,
 * or `GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED` if `buf_cap` is insufficient.
 */
extern gcc_gen_be_error_t gcc_gen_be_encode_uleb128(
    unsigned long value,
    unsigned char *out_buf,
    size_t buf_cap,
    size_t *out_len) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Encodes a signed integer value into DWARF SLEB128 byte format.
 * @details Writes the variable-length SLEB128 representation of `value` into `out_buf`
 * (up to `buf_cap` bytes) and writes the number of bytes written to `out_len`.
 * @param[in] value The signed value to encode.
 * @param[out] out_buf Destination buffer for encoded bytes.
 * @param[in] buf_cap Capacity of `out_buf` in bytes.
 * @param[out] out_len Pointer to receive the number of encoded bytes written.
 * @return GCC_GEN_BE_SUCCESS on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if pointers are NULL,
 * or `GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED` if `buf_cap` is insufficient.
 */
extern gcc_gen_be_error_t gcc_gen_be_encode_sleb128(
    long value,
    unsigned char *out_buf,
    size_t buf_cap,
    size_t *out_len) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a direct call with an abnormal unwind CFG edge to a landing pad.
 *
 * @details When `p_gcc_jit_block_end_with_invoke` is resolved from `libgccjit` and
 * `result_lval` is NULL, emits a native GIMPLE invoke terminator branching to
 * `normal_dest` on normal return and `unwind_dest` on exception unwind. Otherwise,
 * evaluates the call (assigning to `result_lval` if non-NULL), checks
 * `__gcc_be_eh_check_unwind_state()`, and branches to `normal_dest` or `unwind_dest`.
 * Advances the active block to `normal_dest`.
 *
 * @param[in] ctx The active `gcc_jit_context`.
 * @param[in] block The basic block in which the call is emitted.
 * @param[in] fn The target `gcc_jit_function` to invoke.
 * @param[in] num_args Number of arguments in `args`.
 * @param[in] args Array of argument rvalues (may be NULL if `num_args == 0`).
 * @param[in] result_lval Optional destination lvalue for non-void return values (may be NULL).
 * @param[in] normal_dest Normal continuation block (if NULL, a new block is allocated).
 * @param[in] unwind_dest Abnormal unwind landing pad block.
 * @param[out] out_next_block Pointer to receive the normal continuation block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_invoke_call(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_function *fn,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits an indirect call through a function pointer with an abnormal unwind CFG edge.
 *
 * @details Evaluates an indirect function-pointer or virtual-table call, assigning the
 * result to `result_lval` when non-NULL, and terminates `block` with an abnormal unwind
 * edge to `unwind_dest` and normal edge to `normal_dest`. Advances the active block to `normal_dest`.
 *
 * @param[in] ctx The active `gcc_jit_context`.
 * @param[in] block The basic block in which the indirect call is emitted.
 * @param[in] fn_ptr Function pointer rvalue to invoke.
 * @param[in] num_args Number of arguments in `args`.
 * @param[in] args Array of argument rvalues (may be NULL if `num_args == 0`).
 * @param[in] result_lval Optional destination lvalue for non-void return values (may be NULL).
 * @param[in] normal_dest Normal continuation block (if NULL, a new block is allocated).
 * @param[in] unwind_dest Abnormal unwind landing pad block.
 * @param[out] out_next_block Pointer to receive the normal continuation block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_invoke_call_through_ptr(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_rvalue *fn_ptr,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a DWARF landing pad selector comparison against an LSDA filter ID.
 *
 * @details Compares `selector_rval` (from `__builtin_eh_filter(0)`) against `filter_id`:
 * - For `filter_id > 0`, branches to `match_block` when `selector == filter_id`, else to `*out_next_dispatch_block`.
 * - For `filter_id == 0` (`catch (...)`), branches to `match_block`.
 * - For `filter_id < 0` (exception specification filter), branches to `match_block` when `selector < 0`.
 *
 * @param[in] func The active `gcc_jit_function`.
 * @param[in] dispatch_block The current landing pad dispatch block to terminate.
 * @param[in] selector_rval The exception selector rvalue (`int` or pointer castable to `int`).
 * @param[in] filter_id The LSDA filter index (`> 0` typed catch, `0` catch-all, `< 0` exception specification).
 * @param[in] match_block The basic block to jump to when the selector matches.
 * @param[out] out_next_dispatch_block Pointer to receive the newly created fallthrough dispatch block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_selector_filter_dispatch(
    struct gcc_jit_function *func,
    struct gcc_jit_block *dispatch_block,
    struct gcc_jit_rvalue *selector_rval,
    int filter_id,
    struct gcc_jit_block *match_block,
    struct gcc_jit_block **out_next_dispatch_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Synthesizes a partial array construction unwind landing pad.
 *
 * @details Creates a landing pad that iterates backward from the current `index_lval` down to `0`,
 * invoking `elem_dtor` on each already-constructed array element `&base_ptr_lval[index_lval - 1]`
 * before resuming stack unwinding via `_Unwind_Resume` or jumping to an outer landing pad.
 *
 * @param[in] func The active `gcc_jit_function`.
 * @param[in] base_ptr_lval Local lvalue holding the pointer to element 0 of the array.
 * @param[in] index_lval Local `size_t` lvalue tracking the number of constructed elements.
 * @param[in] elem_type Frontend type of a single array element.
 * @param[in] elem_dtor Destructor routine for an array element.
 * @param[out] out_cleanup_lp Pointer to receive the synthesized cleanup landing pad block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_partial_array_cleanup(
    struct gcc_jit_function *func,
    struct gcc_jit_lvalue *base_ptr_lval,
    struct gcc_jit_lvalue *index_lval,
    a_type_ptr elem_type,
    a_routine_ptr elem_dtor,
    struct gcc_jit_block **out_cleanup_lp) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Synthesizes a constructor base/member subobject unwind cleanup landing pad.
 *
 * @details Creates a landing pad block that adjusts `this_ptr` by `subobject_offset` bytes,
 * invokes `subobject_dtor` on the initialized base or member subobject, and chains to the
 * previously active landing pad (or calls `_Unwind_Resume` if none is active).
 *
 * @param[in] func The active constructor `gcc_jit_function`.
 * @param[in] this_ptr The `this` pointer rvalue of the object under construction.
 * @param[in] subobject_offset Byte offset of the base class or member subobject within `this`.
 * @param[in] subobject_dtor Destructor routine (`D2` / `D1`) for the subobject.
 * @param[out] out_cleanup_lp Pointer to receive the synthesized cleanup landing pad block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_subobject_unwind_cleanup(
    struct gcc_jit_function *func,
    struct gcc_jit_rvalue *this_ptr,
    long subobject_offset,
    a_routine_ptr subobject_dtor,
    struct gcc_jit_block **out_cleanup_lp) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Formats active assembler directives for an LSDA call-site and personality/LSDA CFI header.
 * @details Writes `.cfi_personality`, `.cfi_lsda`, and `.uleb128` label-difference directives
 * (`.Ltry_start_N - .Llp_start_N`, `.Ltry_end_N - .Ltry_start_N`, `.Llanding_pad_N - .Llp_start_N`,
 * and `action_idx`) into `out_buf` and stores the string length in `out_len`.
 * @param[in] func_id Unique function EH identifier.
 * @param[in] try_id Region try identifier within the function.
 * @param[in] lp_id Landing pad identifier within the function.
 * @param[in] action_idx 1-based LSDA action table index (0 for cleanup-only).
 * @param[out] out_buf Destination character buffer.
 * @param[in] buf_cap Capacity of `out_buf` in bytes.
 * @param[out] out_len Pointer to receive the number of characters written (excluding NUL).
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if pointers are NULL,
 * or `GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED` if `buf_cap` is too small.
 */
extern gcc_gen_be_error_t gcc_gen_be_format_lsda_asm_directives(
    int func_id,
    int try_id,
    int lp_id,
    int action_idx,
    char *out_buf,
    size_t buf_cap,
    size_t *out_len) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Chains a cleanup action record (`filter = 0`) onto an enclosing catch action chain.
 * @details Appends a SLEB128 `filter = 0` record to the active function's LSDA action table whose
 * relative `next_offset` points to the 1-based `enclosing_action_index` (or 0 if `enclosing_action_index <= 0`),
 * and writes the new 1-based action table index to `out_chained_action_index`.
 * @param[in] enclosing_action_index 1-based action table index of the enclosing catch handler chain (or 0).
 * @param[out] out_chained_action_index Pointer to receive the new 1-based chained action table index.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_chain_cleanup_action(
    int enclosing_action_index,
    int *out_chained_action_index) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits ARM Exception Handling ABI (`.ARM.extab` and `.ARM.exidx`) unwind descriptor tables.
 * @details Declares `__aeabi_unwind_cpp_pr0`, `__aeabi_unwind_cpp_pr1`, and `__cxa_end_cleanup`,
 * and synthesizes `.ARM.extab` (compact model 0 `0x80b0b0b0` or generic model 1 `0x8101b0b0`) and
 * `.ARM.exidx` two-word table globals for `func`.
 * @param[in] func The `gcc_jit_function` owning the ARM EHABI unwind table.
 * @param[in] use_compact_pr0 `TRUE` to emit compact `__aeabi_unwind_cpp_pr0` descriptors, `FALSE` for `__aeabi_unwind_cpp_pr1`.
 * @param[out] out_extab_global Pointer to receive the synthesized `.ARM.extab` global lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_arm_ehabi_tables(
    struct gcc_jit_function *func,
    a_boolean use_compact_pr0,
    struct gcc_jit_lvalue **out_extab_global) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_EH_H */
