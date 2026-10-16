/**
 * @file llvm_gen_be_codegen.h
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
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/ADT/SmallVector.h>
#include <optional>

BEGIN_EDG_NAMESPACE

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
 * @brief Initializes all necessary LLVM target registries.
 * @details Calls InitializeAllTargetInfos(), InitializeAllTargets(), InitializeAllTargetMCs(),
 * InitializeAllAsmPrinters(), and InitializeAllAsmParsers(). This must be called before
 * attempting to create a target machine.
 */
llvm_gen_be_error_t initialize_llvm_targets(void) noexcept;

/**
 * @brief Creates a configured TargetMachine instance.
 * @details Looks up the target based on the triple, sets up TargetOptions, and instantiates
 * a TargetMachine.
 * @param[in] triple_str The target triple string (e.g., "x86_64-pc-linux-gnu").
 * @param[in] cpu The target CPU name (e.g., "generic"). Can be empty.
 * @param[in] features Target feature string (e.g., "+sse2"). Can be empty.
 * @param[in] opt_level Code generation optimization level.
 * @param[in] reloc_model Relocation model (e.g., llvm::Reloc::PIC_).
 * @param[in] code_model Code model (e.g., llvm::CodeModel::Small).
 * @param[out] out_tm Receives the allocated TargetMachine on success.
 * @return llvm_gen_be_error_t::ok on success.
 * @return llvm_gen_be_error_t::invalid_argument if the target triple cannot be resolved.
 * @return llvm_gen_be_error_t::out_of_memory if allocation fails.
 */
llvm_gen_be_error_t create_target_machine(
    const char* triple_str,
    const char* cpu,
    const char* features,
    llvm::CodeGenOptLevel opt_level,
    std::optional<llvm::Reloc::Model> reloc_model,
    std::optional<llvm::CodeModel::Model> code_model,
    llvm::TargetMachine** out_tm) noexcept;

/**
 * @brief Emits the given LLVM module to a file.
 * @details Depending on `file_type`, this may emit native object code, assembly, LLVM bitcode,
 * or LLVM IR text. Object and assembly emission utilize the legacy pass manager.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The TargetMachine configured for the target. Required for object and assembly.
 * @param[in] file_type The format of the output file.
 * @param[in] output_file_path The file path to write to.
 * @return llvm_gen_be_error_t::ok on success.
 * @return llvm_gen_be_error_t::io_error if the output file cannot be opened or written to.
 * @return llvm_gen_be_error_t::code_gen_failure if file emission fails in the pass manager.
 */
llvm_gen_be_error_t emit_machine_code_to_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    codegen_file_type_t file_type,
    const char* output_file_path) noexcept;

/**
 * @brief Emits a direct object file (.o / .obj) from the given module.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The TargetMachine configured for the target.
 * @param[in] output_file_path The file path to write the object file to.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t emit_object_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    const char* output_file_path) noexcept;

/**
 * @brief Emits a direct assembly file (.s / .asm) from the given module.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The TargetMachine configured for the target.
 * @param[in] output_file_path The file path to write the assembly file to.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t emit_assembly_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    const char* output_file_path) noexcept;

/**
 * @brief Emits the machine code to a streaming memory buffer.
 * @param[in] module The LLVM module to emit.
 * @param[in] tm The TargetMachine configured for the target.
 * @param[in] file_type The type of machine code to emit (assembly or object).
 * @param[out] out_buffer The emitted memory buffer on success.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t emit_machine_code_to_memory_buffer(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    codegen_file_type_t file_type,
    llvm::SmallVectorImpl<char>& out_buffer) noexcept;

/**
 * @brief Generates a ThinLTO summary index for the given module directly in the backend driver.
 * @param[in] module The LLVM module.
 * @param[in] output_file_path The file path to write the ThinLTO index bitcode file to.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t emit_thinlto_summary(
    llvm::Module* module,
    const char* output_file_path) noexcept;

/**
 * @brief Enables Split DWARF (.dwo) output during native object emission.
 * @param[in] tm The TargetMachine configured for the target.
 * @param[in] dwo_output_path The DWO file output path.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t enable_split_dwarf_emission(
    llvm::TargetMachine* tm,
    const char* dwo_output_path) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_CODEGEN_H */
