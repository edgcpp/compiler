/**
 * @file llvm_gen_be_codegen.h
 * @ingroup llvm_gen_be_driver
 * @brief LLVM Code Generation and Machine Code Emission
 * @details This file provides interfaces for target machine initialization,
 * target registry lookup, and direct emission of object code, assembly, and bitcode.
 * It encapsulates the LLVM MC (Machine Code) layer and the legacy pass manager used
 * for file emission.
 * 
 * Thread Safety: The routines in this module are generally thread-safe provided
 * the underlying LLVM contexts and modules are not mutated concurrently. Target
 * registry initialization must be performed once sequentially.
 */

#ifndef LLVM_GEN_BE_CODEGEN_H
#define LLVM_GEN_BE_CODEGEN_H

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/Config/llvm-config.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/ADT/SmallVector.h>
#include <optional>

BEGIN_EDG_NAMESPACE

#if LLVM_VERSION_MAJOR >= 18
/** @brief Version-portable alias for LLVM TargetMachine code-generation optimization level (LLVM 18+). */
using llvm_codegen_opt_level_t = llvm::CodeGenOptLevel;
#else
/** @brief Version-portable alias for LLVM TargetMachine code-generation optimization level (LLVM < 18). */
using llvm_codegen_opt_level_t = llvm::CodeGenOpt::Level;
#endif

/**
 * @enum codegen_file_type_t
 * @brief Defines the output file formats supported by the code generator.
 */
enum class codegen_file_type_t {
    assembly_file, ///< Assembly code file (.s)
    object_file,   ///< Machine object file (.o / .obj)
    bitcode_file,  ///< LLVM bitcode file (.bc)
    llvm_ir_text   ///< LLVM IR text file (.ll)
};

/**
 * @brief Initializes all LLVM target info, target machine, MC, asm printer, and asm parser registries.
 * @details Must be called before looking up targets or creating `TargetMachine` instances.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t initialize_llvm_targets(void) noexcept;

/**
 * @brief Creates or retrieves a cached LLVM TargetMachine for the specified target configuration.
 * @details Looks up the target in `TargetRegistry` and caches the `TargetMachine` keyed by triple, CPU, features, and models.
 * @param[in] triple_str The target triple string.
 * @param[in] cpu The target CPU name (or nullptr for "generic").
 * @param[in] features The target feature string (or nullptr for none).
 * @param[in] opt_level The LLVM code generation optimization level.
 * @param[in] reloc_model Optional relocation model.
 * @param[in] code_model Optional code model.
 * @param[out] out_tm Receives the resulting `llvm::TargetMachine*` on success.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t create_target_machine(
    const char* triple_str,
    const char* cpu,
    const char* features,
    llvm_codegen_opt_level_t opt_level,
    std::optional<llvm::Reloc::Model> reloc_model,
    std::optional<llvm::CodeModel::Model> code_model,
    llvm::TargetMachine** out_tm) noexcept;

/**
 * @brief Emits the LLVM module to a file in the specified representation (IR text, bitcode, assembly, or object).
 * @details Opens `output_file_path` and writes textual IR, bitcode, or target machine code.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The configured `TargetMachine` (required for assembly/object emission).
 * @param[in] file_type The requested output file format.
 * @param[in] output_file_path Destination file path on disk.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t emit_machine_code_to_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    codegen_file_type_t file_type,
    const char* output_file_path) noexcept;

/**
 * @brief Emits the LLVM module directly to a native object file (`.o` / `.obj`).
 * @details Delegates to `emit_machine_code_to_file` with `codegen_file_type_t::object_file`.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The configured `TargetMachine`.
 * @param[in] output_file_path Destination object file path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t emit_object_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    const char* output_file_path) noexcept;

/**
 * @brief Emits the LLVM module directly to a target assembly file (`.s` / `.asm`).
 * @details Delegates to `emit_machine_code_to_file` with `codegen_file_type_t::assembly_file`.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The configured `TargetMachine`.
 * @param[in] output_file_path Destination assembly file path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t emit_assembly_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    const char* output_file_path) noexcept;

/**
 * @brief Emits the machine code to a streaming memory buffer.
 * @details Translates the given module into assembly or object code bytes stored in `out_buffer`.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The TargetMachine configured for the target.
 * @param[in] file_type The type of machine code to emit (assembly or object).
 * @param[out] out_buffer Pointer to the buffer that receives the emitted machine code on success.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t emit_machine_code_to_memory_buffer(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    codegen_file_type_t file_type,
    llvm::SmallVectorImpl<char>* out_buffer) noexcept;

/**
 * @brief Clears and releases all cached TargetMachine instances.
 * @details Safely destroys all `TargetMachine` objects stored in the internal cache.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t release_target_machine_cache(void) noexcept;

/**
 * @brief Emits a ThinLTO bitcode file containing module summary index records.
 * @details Runs `ThinLTOBitcodeWriterPass` on `module` and writes the output to `output_file_path`.
 * @param[in] module The LLVM module to summarize and write.
 * @param[in] output_file_path The destination ThinLTO bitcode file path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t emit_thinlto_summary(
    llvm::Module* module,
    const char* output_file_path) noexcept;

/**
 * @brief Emits a ThinLTO or FullLTO bitcode file containing module summary index records.
 * @details Delegates to `emit_thinlto_summary` when @p thin_lto is true, or writes standard
 *          LTO bitcode via `emit_machine_code_to_file` when @p thin_lto is false.
 * @param[in] module The LLVM module to serialize.
 * @param[in] output_path Destination bitcode file path.
 * @param[in] thin_lto `true` to include ThinLTO module summary index records, `false` for FullLTO bitcode.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t emit_llvm_lto_bitcode_file(
    llvm::Module* module,
    const char* output_path,
    bool thin_lto) noexcept;

/**
 * @brief Configures Split DWARF (`.dwo`) emission on the given TargetMachine.
 * @details Sets `MCOptions.SplitDwarfFile` to `dwo_output_path`.
 * @param[in,out] tm The TargetMachine to configure.
 * @param[in] dwo_output_path Path to the `.dwo` output file.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t enable_split_dwarf_emission(
    llvm::TargetMachine* tm,
    const char* dwo_output_path) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_CODEGEN_H */
