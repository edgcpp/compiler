/**
 * @file llvm_gen_be_codegen.cpp
 * @brief Implementation of LLVM Code Generation and Machine Code Emission
 * @details Implements target machine initialization and file emission routines.
 */

#include "llvm_gen_be_codegen.h"
#include "llvm_gen_be_internal.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <string>

BEGIN_EDG_NAMESPACE

/**
 * @brief initialize_llvm_targets
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
llvm_gen_be_error_t initialize_llvm_targets(void) noexcept {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t create_target_machine(
    const char* triple_str,
    const char* cpu,
    const char* features,
    llvm::CodeGenOptLevel opt_level,
    llvm::TargetMachine** out_tm) noexcept
{
    if (!triple_str || !out_tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for triple_str or out_tm");
    }

    std::string error_str;
    llvm::Triple theTriple(triple_str);
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(theTriple, error_str);
    if (!target) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Failed to lookup target: %s", error_str.c_str());
    }

    llvm::TargetOptions opt;

    llvm::TargetMachine* tm = target->createTargetMachine(
        theTriple,
        cpu ? cpu : "generic",
        features ? features : "",
        opt,
        llvm::Reloc::PIC_,
        std::nullopt,
        opt_level);

    if (!tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::out_of_memory, __FILE__, __LINE__, 0, "Failed to allocate TargetMachine");
    }

    *out_tm = tm;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t emit_machine_code_to_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    codegen_file_type_t file_type,
    const char* output_file_path) noexcept
{
    if (!module || !output_file_path) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for module or output_file_path");
    }

    std::error_code ec;
    llvm::sys::fs::OpenFlags open_flags = llvm::sys::fs::OF_None;
    if (file_type == codegen_file_type_t::llvm_ir_text || file_type == codegen_file_type_t::assembly_file) {
        open_flags = llvm::sys::fs::OF_Text;
    }

    llvm::raw_fd_ostream dest(output_file_path, ec, open_flags);
    if (ec) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::io_error, __FILE__, __LINE__, 0, "Could not open output file: %s", ec.message().c_str());
    }

    if (file_type == codegen_file_type_t::llvm_ir_text) {
        module->print(dest, nullptr);
        if (dest.has_error()) {
            return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::io_error, __FILE__, __LINE__, 0, "Failed to write LLVM IR text to file");
        }
        return llvm_gen_be_error_t::ok;
    }

    if (file_type == codegen_file_type_t::bitcode_file) {
        llvm::WriteBitcodeToFile(*module, dest);
        if (dest.has_error()) {
            return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::io_error, __FILE__, __LINE__, 0, "Failed to write bitcode to file");
        }
        return llvm_gen_be_error_t::ok;
    }

    if (!tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "TargetMachine is required for object and assembly emission");
    }

    llvm::CodeGenFileType cg_file_type;
    switch (file_type) {
        case codegen_file_type_t::assembly_file:
            // Workaround for older and newer LLVM versions. In newer LLVM, CodeGenFileType is in llvm namespace directly or CodeGenFileType::AssemblyFile
            cg_file_type = llvm::CodeGenFileType::AssemblyFile;
            break;
        case codegen_file_type_t::object_file:
            cg_file_type = llvm::CodeGenFileType::ObjectFile;
            break;
        default:
            return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Unknown codegen_file_type_t");
    }

    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr, cg_file_type)) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::code_gen_failure, __FILE__, __LINE__, 0, "Target machine can't emit a file of this type");
    }

    pass.run(*module);

    dest.flush();
    if (dest.has_error()) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::io_error, __FILE__, __LINE__, 0, "Failed to write or flush machine code to file");
    }

    return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
