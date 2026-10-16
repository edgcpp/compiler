/**
 * @file llvm_gen_be_internal.h
 * @brief Internal headers.
 */
#ifndef LLVM_GEN_BE_INTERNAL_H
#define LLVM_GEN_BE_INTERNAL_H

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be.h"
#include "llvm_gen_be_error.h"
#include "host_envir.h"
#include "error.h"
#include "il.h"
#include "targ_def.h"
#include "types.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_const.h"
#include "llvm_gen_be_expr.h"
#include "llvm_gen_be_debug.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/TargetParser/Triple.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

BEGIN_EDG_NAMESPACE

struct llvm_gen_be_debug_state_t;

/**
 * @struct LLVMBackendState
 * @brief Global state for the LLVM generation backend.
 * @details Encapsulates all persistent data structures, caches, and context needed
 * during the lowering of EDG IL to LLVM IR.
 */

/**
 * @struct ScopeCleanup
 * @brief Represents an active automatic variable requiring RAII cleanup.
 */
struct ScopeCleanup {
    a_routine_ptr dtor_routine; ///< The destructor routine to invoke.
    llvm::Value* object_ptr;    ///< Pointer to the object to be destroyed.
    llvm::Function* finally_func = nullptr; ///< The outlined __finally funclet to call.
    llvm::BasicBlock* normal_dest; ///< The basic block to branch to after normal cleanup.
    bool is_active;             ///< Whether this cleanup is currently active in the scope.
    bool is_guard_abort = false;///< True if this is a thread-safe static guard abort.
    llvm::Value* guard_abort_ptr = nullptr; ///< Guard variable pointer to pass to __cxa_guard_abort.
};

/**
 * @struct ScopeCleanupStack
 * @brief Manages the stack of active cleanups for RAII.
 */
struct ScopeCleanupStack {
    std::vector<ScopeCleanup> cleanups;
};

struct LLVMBackendState {
  std::unique_ptr<llvm::LLVMContext> context;       ///< LLVM context instance.
  std::unique_ptr<llvm::Module> module;             ///< Top-level LLVM module.
  std::unique_ptr<llvm::IRBuilder<>> builder;       ///< IR instruction builder.
  std::unordered_map<a_type_ptr, llvm::Type*> type_cache; ///< Cache mapping EDG a_type_ptr to llvm::Type*.
  std::unordered_map<a_variable_ptr, llvm::Value*> local_vars; ///< Symbol table mapping EDG variables to allocas/values.
  std::unordered_map<a_variable_ptr, llvm::Value*> vla_saved_stacks; ///< VLA stack pointer storage for \@llvm.stacksave.
  std::unordered_map<a_type_ptr, a_vla_dimension_ptr> array_to_vla_dim; ///< Dimension map for variable-length arrays.
  std::unordered_map<a_vla_dimension_ptr, llvm::Value*> vla_dimension_values; ///< Evaluated dimension values.
  std::unordered_map<a_dynamic_init_ptr, llvm::Value*> reused_values; ///< Map for enk_reuse_value cache mappings.

  std::vector<llvm::BasicBlock*> break_blocks;      ///< Stack of target basic blocks for break.
  std::vector<llvm::BasicBlock*> continue_blocks;   ///< Stack of target basic blocks for continue.
  std::vector<llvm::BasicBlock*> current_landing_pads; ///< Stack of active exception landing pads.
  std::vector<llvm::Value*> active_funclet_pads;    ///< Stack of active Windows SEH funclet pads (catchpad/cleanuppad).
  
  std::vector<llvm::Value*> escaped_allocas;        ///< Allocas that need llvm.localescape.
  std::unordered_map<llvm::Value*, int> escaped_var_indices; ///< Indices of escaped allocas.

  ScopeCleanupStack cleanup_stack; ///< RAII scope cleanup stack.
  std::unordered_map<a_label_ptr, llvm::BasicBlock*> label_blocks; ///< Map from EDG labels to LLVM basic blocks.
  std::unordered_map<a_switch_case_entry_ptr, llvm::BasicBlock*> case_blocks; ///< Map from switch case entries to LLVM basic blocks.
  std::unordered_map<std::string, llvm::GlobalVariable*> string_literal_cache; ///< Deduplication cache for string literal globals.
  
  llvm_gen_be_debug_state_t* dbg_state = nullptr;   ///< Active DWARF debug state handle.
  llvm_gen_be_error_context_t err_context;          ///< Persistent diagnostic context buffer.
  
  llvm::Value* current_stmt_expr_result_ptr = nullptr; ///< Current temporary allocation for GNU statement expressions.
};

extern LLVMBackendState* be_state;

/**
 * @brief Retrieves or lowers an EDG type to an LLVM Type.
 * @param[in] edg_type The EDG type to lower.
 * @param[out] out_type Receives the lowered LLVM type on success.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t get_llvm_type(a_type_ptr edg_type, llvm::Type** out_type) noexcept;

/**
 * @brief Retrieves or generates an LLVM global for the typeinfo of a given type.
 * @param[in] type The EDG type to get typeinfo for.
 * @param[out] out_const Receives the LLVM constant global on success.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers an EDG statement into LLVM IR instructions.
 * @param[in] stmt The EDG statement to lower.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers all global variables in the translation unit.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_lower_global_variables(void) noexcept;

/**
 * @brief Lowers all function declarations (prototypes) in the translation unit.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_lower_function_declarations(void) noexcept;

/**
 * @brief Lowers all function definitions (bodies) in the translation unit.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_lower_function_definitions(void) noexcept;
llvm_gen_be_error_t llvm_lower_module_metadata(void) noexcept;


/**
 * @brief Lowers global constructors and destructors.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors(void) noexcept;

/**
 * @brief Builds the LLVM data layout string based on EDG target macros.
 * @param[out] out_dl Receives the data layout string.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t build_data_layout(std::string* out_dl) noexcept;

/**
 * @brief Verifies that the backend state has no unterminated basic blocks.
 * @details Iterates over all generated functions and basic blocks to ensure
 * there are no dangling basic blocks without terminators before module finalization.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::internal_inconsistency.
 */
llvm_gen_be_error_t llvm_gen_be_verify_state(void) noexcept;

/**
 * @brief Emits a dynamic initialization to a given pointer.
 */
llvm_gen_be_error_t llvm_emit_dynamic_init(a_dynamic_init_ptr dip, llvm::Value* ptr, llvm::Type* obj_type) noexcept;

llvm_gen_be_error_t llvm_lower_module_metadata(void) noexcept;


/**
 * @brief Lowers global constructors and destructors.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors(void) noexcept;

/**
 * @brief Builds the LLVM data layout string based on EDG target macros.
 * @param[out] out_dl Receives the data layout string.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t build_data_layout(std::string* out_dl) noexcept;

/**
 * @brief Verifies that the backend state has no unterminated basic blocks.
 * @details Iterates over all generated functions and basic blocks to ensure
 * there are no dangling basic blocks without terminators before module finalization.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::internal_inconsistency.
 */
llvm_gen_be_error_t llvm_gen_be_verify_state(void) noexcept;

/**
 * @brief Emits a dynamic initialization to a given pointer.
 */
llvm_gen_be_error_t llvm_emit_dynamic_init(a_dynamic_init_ptr dip, llvm::Value* ptr, llvm::Type* obj_type) noexcept;

END_EDG_NAMESPACE

#endif
