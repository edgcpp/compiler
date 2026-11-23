/**
 * @defgroup llvm_gen_be_debug DWARF & CodeView Debug Info
 * @ingroup llvm_gen_be
 * @brief DWARF and CodeView debug metadata emission subsystem.
 */

/**
 * @file llvm_gen_be_debug.h
 * @ingroup llvm_gen_be_debug
 * @brief LLVM IR Backend Debug Information (DWARF/CodeView) Subsystem.
 * @details Encapsulates LLVM's DIBuilder and manages debug metadata emission
 * including compile units, lexical scopes, types, and source locations.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_DEBUG_H
#define LLVM_GEN_BE_DEBUG_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <unordered_map>
#include <vector>
#include <string>

BEGIN_EDG_NAMESPACE

/**
 * @struct llvm_gen_be_debug_state_t
 * @brief Manages the state and context for LLVM debug information generation.
 * @details Holds the active `DIBuilder`, the current `DICompileUnit`, and caches
 * for files, types, and lexical scopes.
 */
struct llvm_gen_be_debug_state_t {
  llvm::DIBuilder* builder;                                     ///< Pointer to the LLVM DIBuilder instance.
  llvm::DICompileUnit* compile_unit;                            ///< The main compile unit for the module.
  std::unordered_map<std::string, llvm::DIFile*> di_files_map;  ///< Cache mapping file paths to their LLVM DIFile descriptors.
  std::unordered_map<a_type_ptr, llvm::DIType*> di_types_map;   ///< Cache mapping EDG types to their LLVM DIType descriptors.
  std::vector<llvm::DIScope*> scope_stack;                      ///< Stack of current lexical scopes (DIScope*).
  bool is_codeview;                                             ///< True if CodeView metadata should be emitted instead of DWARF.
  unsigned next_param_arg_no = 1;                               ///< Next 1-based DWARF parameter index in the current subprogram.
};

/**
 * @brief Initializes the debug information generation subsystem.
 * @details Creates the DIBuilder instance, emits compile unit metadata, and sets DWARF/CodeView module flags.
 * @param[out] out_dbg_state Receives the allocated debug state handle on success.
 * @param[in] module The LLVM Module being compiled.
 * @param[in] source_file The primary source file path.
 * @param[in] comp_dir The compilation working directory.
 * @param[in] is_optimized True if optimizations are enabled.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t debug_info_init(
    llvm_gen_be_debug_state_t** out_dbg_state,
    llvm::Module* module,
    const char* source_file,
    const char* comp_dir,
    bool is_optimized) noexcept;

/**
 * @brief Finalizes the debug information generator.
 * @details Invokes DIBuilder::finalize to seal all debug info metadata nodes.
 * @param[in] dbg_state Pointer to the active debug state handle.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if null.
 */
llvm_gen_be_error_t debug_info_finalize(llvm_gen_be_debug_state_t* dbg_state) noexcept;

/**
 * @brief Releases all allocated memory and resources associated with debug info generation.
 * @details Frees the DIBuilder and clears maps and scope stacks.
 * @param[in,out] dbg_state Pointer to the debug state pointer to clean up and nullify.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointer is null.
 */
llvm_gen_be_error_t debug_info_cleanup(llvm_gen_be_debug_state_t** dbg_state) noexcept;

/**
 * @brief Retrieves or creates an LLVM DIFile metadata node for a given source file path.
 * @details Hashes file contents with MD5 checksum if available and caches DIFile instances.
 * @param[in,out] dbg_state Pointer to the active debug state handle.
 * @param[in] file_path The path to the source file.
 * @param[out] out_di_file Receives the resulting DIFile metadata pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_or_create_di_file(
    llvm_gen_be_debug_state_t* dbg_state,
    const char* file_path,
    llvm::DIFile** out_di_file) noexcept;

/**
 * @brief Gets or creates a `DILocation` for a given source position and scope.
 * @details Maps the EDG sequence number to file and line coordinates and returns an `llvm::DILocation`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] src_pos The EDG source position.
 * @param[in] scope The DWARF scope to associate the location with.
 * @param[out] out_loc Pointer to the resulting `DILocation`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_di_location(
    llvm_gen_be_debug_state_t* dbg_state,
    a_source_position src_pos,
    llvm::DIScope* scope,
    llvm::DILocation** out_loc) noexcept;

/**
 * @brief Applies debug location metadata to an instruction.
 * @details Attaches a `DILocation` from the current top lexical scope on `dbg_state->scope_stack`.
 * @param[in] inst The LLVM instruction.
 * @param[in] src_pos The EDG source position.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t apply_instruction_debug_loc(
    llvm::Instruction* inst,
    a_source_position src_pos) noexcept;

/**
 * @brief Creates a `DISubprogram` for a given routine and attaches it to the function.
 * @details Resolves source file/line information, sets subprogram flags, attaches the subprogram
 *          to `fn`, and pushes it onto `dbg_state->scope_stack`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] routine The EDG routine.
 * @param[in] fn The LLVM function.
 * @param[out] out_subprogram Pointer to the resulting `DISubprogram`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t create_di_subprogram(
    llvm_gen_be_debug_state_t* dbg_state,
    a_routine_ptr routine,
    llvm::Function* fn,
    llvm::DISubprogram** out_subprogram) noexcept;

/**
 * @brief Pushes a new lexical block onto the scope stack.
 * @details Creates a `DILexicalBlock` nested under the current top scope and pushes it onto `scope_stack`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] src_pos The EDG source position.
 * @param[out] out_block Pointer to the resulting `DILexicalBlock`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t push_lexical_block(
    llvm_gen_be_debug_state_t* dbg_state,
    a_source_position src_pos,
    llvm::DILexicalBlock** out_block) noexcept;

/**
 * @brief Pops the current lexical block from the scope stack.
 * @details Removes the top `DIScope*` entry from `dbg_state->scope_stack`.
 * @param[in,out] dbg_state The debug state.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if empty or null.
 */
llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg_state) noexcept;

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
    llvm::DIType** out_di_type) noexcept;

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
    llvm::AllocaInst* alloca_inst) noexcept;

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
    llvm::BasicBlock* insert_at_end = nullptr,
    const uint64_t* expr_ops = nullptr,
    size_t num_expr_ops = 0) noexcept;

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
    llvm::Instruction* insert_before) noexcept;

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
    llvm::GlobalVariable* gv) noexcept;

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
    llvm::DIDerivedType** out_inheritance) noexcept;

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
    llvm::DISubprogram** out_method) noexcept;

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
    llvm::DINamespace** out_namespace) noexcept;

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
    llvm::DITemplateTypeParameter** out_param) noexcept;

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
    llvm::DITemplateValueParameter** out_param) noexcept;

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
    llvm::DICompositeType** out_enum) noexcept;

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
    llvm::DIImportedEntity** out_import) noexcept;

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
    llvm::DIImportedEntity** out_import) noexcept;

/**
 * @brief Emits DWARF macro debug entries (`DW_MACINFO_define` / `DW_MACINFO_undef`) into a `DIMacroFile`.
 * @details Creates a `DIMacroFile` under @p dbg_state and populates a `DIMacro` entry for
 *          @p macro_name and @p macro_value (`DW_MACINFO_define` when @p is_undef is false,
 *          or `DW_MACINFO_undef` when @p is_undef is true).
 * @param[in,out] dbg_state The active debug state.
 * @param[in] line_no Source line number of the macro directive.
 * @param[in] macro_name Null-terminated macro identifier.
 * @param[in] macro_value Optional macro expansion string (may be null).
 * @param[in] is_undef `true` to emit `DW_MACINFO_undef`, `false` for `DW_MACINFO_define`.
 * @param[out] out_macro Receives the created `DIMacro*` node.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t emit_di_macro_info(
    llvm_gen_be_debug_state_t* dbg_state,
    unsigned line_no,
    const char* macro_name,
    const char* macro_value,
    bool is_undef,
    llvm::DIMacro** out_macro) noexcept;

/**
 * @brief Emits `DW_TAG_imported_module` and `DW_TAG_imported_declaration` debug records for a scope.
 * @details Inspects `scope->namespaces` and `scope->routines` to emit `DIImportedEntity` metadata
 *          nodes in @p dbg_state and appends their count to `*out_count`.
 * @param[in,out] dbg_state The active debug state.
 * @param[in] scope The EDG scope to inspect.
 * @param[out] out_count Receives the number of imported entity debug nodes emitted.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t emit_scope_imported_entities(
    llvm_gen_be_debug_state_t* dbg_state,
    a_scope_ptr scope,
    unsigned* out_count) noexcept;

/**
 * @brief Constructs CodeView/PDB lexical scope, subprogram (`S_GPROC32`/`S_LPROC32`),
 *        local variable (`S_REGREL32`), and inlined site metadata nodes for a scope.
 * @details Translates an EDG `a_scope_ptr` into the corresponding `llvm::DIScope*`
 *          and emits local variable debug records for CodeView/PDB emission.
 * @param[in] scope Pointer to the EDG scope entry.
 * @param[out] out_node Receives the constructed `llvm::DIScope*` metadata node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_pdb_symbol_records(
    a_scope_ptr scope,
    llvm::DIScope** out_node) noexcept;

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
    llvm::DIExpression** out_expr) noexcept;

/**
 * @brief Emits a DWARF `DICommonBlock` debug metadata node (`DW_TAG_common_block`).
 * @details Wraps `DIBuilder::createCommonBlock` to describe a named common storage block.
 * @param[in,out] dbg_state The active debug state.
 * @param[in] scope Enclosing lexical `DIScope*` (or `compile_unit` if null).
 * @param[in] decl `DIGlobalVariable*` declaration associated with the common block.
 * @param[in] name Null-terminated common block identifier.
 * @param[in] file `DIFile*` where the common block is declared.
 * @param[in] line_no 1-based source line number.
 * @param[out] out_block Receives the created `llvm::DICommonBlock*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t emit_debug_common_block(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    llvm::DIGlobalVariable* decl,
    const char* name,
    llvm::DIFile* file,
    unsigned line_no,
    llvm::DICommonBlock** out_block) noexcept;

/**
 * @brief Constructs a chained `DILocation` metadata node for an inlined call site (`inlinedAt`).
 * @details Creates an `llvm::DILocation` at (`line`, `col`) inside `inline_scope` chained to `call_site_loc`.
 * @param[in] dbg_state The active debug state.
 * @param[in] line 1-based line number inside the inlined routine.
 * @param[in] col 1-based column number inside the inlined routine.
 * @param[in] inline_scope Lexical `DIScope*` of the inlined routine.
 * @param[in] call_site_loc Outer `DILocation*` representing the call site (`inlinedAt`).
 * @param[out] out_loc Receives the chained `llvm::DILocation*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t create_inlined_at_debug_location(
    llvm_gen_be_debug_state_t* dbg_state,
    unsigned line,
    unsigned col,
    llvm::DIScope* inline_scope,
    llvm::DILocation* call_site_loc,
    llvm::DILocation** out_loc) noexcept;

END_EDG_NAMESPACE


#endif /* LLVM_GEN_BE_DEBUG_H */
