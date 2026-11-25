/**
 * @defgroup llvm_gen_be_eh Exception Handling & Unwinding
 * @ingroup llvm_gen_be
 * @brief Exception handling and unwinding subsystem for the EDG LLVM backend.
 */

/**
 * @file llvm_gen_be_eh.h
 * @ingroup llvm_gen_be_eh
 * @brief Exception Handling subsystem for the EDG LLVM backend.
 * @details Implements C++ exception lowering for both Itanium ABI and MSVC SEH.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_EH_H
#define LLVM_GEN_BE_EH_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers a C++ `try` / `catch` block statement (`stmk_try_block`).
 * @details Emits MSVC C++ EH (`__CxxFrameHandler3`, `catchswitch`, `catchpad`, `cleanuppad` with
 *          funclet-bound RAII destructor calls) on Windows MSVC targets, or Itanium C++ ABI EH
 *          (`__gxx_personality_v0`, `landingpad`, `__cxa_begin_catch`, `__cxa_end_catch`) otherwise,
 *          binding any caught exception parameter variable in `be_state->local_vars`.
 * @param[in] stmt Pointer to the EDG try-block statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_try_block_stmt(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers a Microsoft SEH `__try` / `__except` or `__try` / `__finally` statement.
 * @details Outlines filter expressions and `__finally` cleanup blocks into funclets using
 *          `llvm.localrecover` and `llvm.eh.recoverfp`, and emits `catchswitch` / `catchpad`
 *          or `cleanuppad` instructions for `__C_specific_handler`.
 * @param[in] stmt Pointer to the EDG statement node (`stmk_microsoft_try`).
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_microsoft_try_stmt(a_statement_ptr stmt) noexcept;

/**
 * @brief Emits an RAII exception cleanup registration for a destructor call.
 * @details Appends a new scope cleanup record to the active cleanup stack.
 * @param[in] dtor The EDG routine pointer for the destructor.
 * @param[in] obj_ptr The LLVM value pointing to the object instance.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept;

/**
 * @brief Lowers the Microsoft SEH `_exception_code()` intrinsic.
 * @details Calls `@llvm.eh.exceptioncode` bound to the active `catchpad` token.
 * @param[out] out_val Receives the 32-bit exception code LLVM Value.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_val is null.
 */
llvm_gen_be_error_t llvm_lower_eh_exceptioncode(llvm::Value** out_val) noexcept;

/**
 * @brief Lowers the Microsoft SEH `_exception_info()` intrinsic.
 * @details Retrieves the exception pointers structure from the filter funclet's first argument.
 * @param[out] out_val Receives the exception info pointer LLVM Value.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_val is null.
 */
llvm_gen_be_error_t llvm_lower_eh_exceptioninfo(llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a Windows SEH-aware `setjmp` call.
 * @details Passes the current frame address from `@llvm.frameaddress.p0` to `_setjmp`.
 * @param[in] jmp_buf The jump buffer pointer argument.
 * @param[out] out_val Receives the result of the `_setjmp` call.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t llvm_lower_seh_setjmp(llvm::Value* jmp_buf, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a Windows SEH-aware `longjmp` call.
 * @details Emits a call to `longjmp` with the jump buffer and status value.
 * @param[in] jmp_buf The jump buffer pointer argument.
 * @param[in] val The integer status value to restore.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t llvm_lower_seh_longjmp(llvm::Value* jmp_buf, llvm::Value* val) noexcept;

/**
 * @brief Emits a landing pad for `noexcept` or dynamic exception specifications (`throw(T1, T2)`).
 * @details Generates filter clauses on the landing pad and dispatches to `std::terminate`
 *          or `__cxa_call_unexpected` on specification violation.
 * @param[in] routine Pointer to the EDG routine node with an exception specification.
 * @param[in] lpad_bb Pointer to the landing pad basic block to populate.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_exception_specification_lpad(
    a_routine_ptr routine,
    llvm::BasicBlock* lpad_bb) noexcept;

/**
 * @brief Attaches Itanium `noexcept` / `throw(...)` filter clauses and termination/unexpected dispatch onto an existing landing pad.
 * @details Inspects @p routine for a non-`throw_any` exception specification. When present, appends the
 *          appropriate filter clause (`[0 x ptr]` for `noexcept`, or `[N x ptr]` typeinfo list for `throw(...)`)
 *          to @p lpad, emits `__clang_call_terminate` or `__cxa_call_unexpected`, terminates the block with
 *          `unreachable`, and sets `*out_handled = true`.
 * @param[in] routine Optional EDG routine whose exception specification is inspected.
 * @param[in,out] lpad LandingPadInst to attach filter clauses to.
 * @param[out] out_handled Receives `true` if a termination/unexpected sequence was emitted.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_routine_exc_spec_handler(
    a_routine_ptr routine,
    llvm::LandingPadInst* lpad,
    bool* out_handled) noexcept;

/**
 * @brief Appends the innermost enclosing `try` block's `catch` typeinfo clauses onto an inner chained `LandingPadInst`.
 * @details Ensures `__gxx_personality_v0` computes matching `@llvm.eh.typeid.for` selector values when an inner
 *          cleanup landing pad branches to `be_state->current_lpad_dispatch_bbs.back()`.
 * @param[in,out] lpad Chained inner `LandingPadInst` to populate with `catch` clauses.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_append_enclosing_try_clauses(
    llvm::LandingPadInst* lpad) noexcept;

/**
 * @brief Creates a chained cleanup landing pad block for active `ScopeCleanup` entries.
 * @details Emits an Itanium `landingpad cleanup` + `resume` or MSVC `cleanuppad` + `cleanupret`
 *          basic block that executes active destructors down to @p target_cleanup_depth.
 * @param[in] target_cleanup_depth Cleanup stack depth to unwind down to.
 * @param[in] next_lpad_bb Optional enclosing landing pad / unwind destination block.
 * @param[out] out_cleanup_lpad_bb Receives the created cleanup landing pad basic block.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_cleanup_landing_pad(
    size_t target_cleanup_depth,
    llvm::BasicBlock* next_lpad_bb,
    llvm::BasicBlock** out_cleanup_lpad_bb) noexcept;

/**
 * @brief Retrieves or lazily materializes the unwind destination basic block for the current cleanup and landing pad state.
 * @details If active `ScopeCleanup` entries exist above the innermost enclosing landing pad depth,
 *          emits a chained cleanup landing pad via `llvm_emit_cleanup_landing_pad`; otherwise returns
 *          the innermost enclosing landing pad (or `nullptr` if none).
 * @param[out] out_unwind_bb Receives the unwind destination basic block, or `nullptr` if no unwind handler is active.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_get_or_create_current_unwind_dest(
    llvm::BasicBlock** out_unwind_bb) noexcept;

/**
 * @brief Retrieves or synthesizes the Itanium ABI `__clang_call_terminate` helper function.
 * @details Synthesizes a `linkonce_odr hidden` `noreturn` `nounwind` function `__clang_call_terminate(ptr)`
 *          that invokes `__cxa_begin_catch` followed by `_ZSt9terminatev` (`std::terminate`) and `unreachable`.
 * @param[out] out_fn Receives the `__clang_call_terminate` `llvm::Function*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_get_or_create_clang_call_terminate(
    llvm::Function** out_fn) noexcept;

/**
 * @brief Lowers a Microsoft SEH `__leave` statement inside a `__try` block.
 * @details Emits an unconditional branch to the innermost active `__try` leave continuation block
 *          stored in `be_state->seh_leave_blocks`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if no SEH `__try` block is active.
 */
llvm_gen_be_error_t llvm_lower_seh_leave(void) noexcept;

/**
 * @brief Retrieves or synthesizes the MSVC C++ EH `_ThrowInfo` (`_TI...`) global for a thrown type.
 * @details Emits `_CT??_R0...` (`_s__CatchableType`) descriptors for @p thrown_type and its public
 *          base classes, the `_CTA...` (`_s__CatchableTypeArray`) table, and the top-level `_TI...`
 *          (`_s__ThrowInfo`) descriptor referencing @p dtor_fn.
 * @param[in] thrown_type The EDG type of the thrown exception object.
 * @param[in] dtor_fn Optional destructor `llvm::Constant*` (or null pointer constant).
 * @param[out] out_throw_info Receives the `_TI...` `llvm::Constant*` global pointer.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_get_or_create_msvc_throw_info(
    a_type_ptr thrown_type,
    llvm::Constant* dtor_fn,
    llvm::Constant** out_throw_info) noexcept;

/**
 * @brief Attaches MSVC `/EHa` asynchronous SEH attributes (`seh-asynch` and `non-call-exceptions`) to a function.
 * @details Configures @p func for hardware trap unwinding under `/EHa` asynchronous exception mode.
 * @param[in,out] func Pointer to the target `llvm::Function`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p func is null.
 */
llvm_gen_be_error_t llvm_enable_async_seh_attributes(llvm::Function* func) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_EH_H */
