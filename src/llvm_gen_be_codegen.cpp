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
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/IPO/ThinLTOBitcodeWriter.h>
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

BEGIN_EDG_NAMESPACE

namespace {
    std::mutex tm_cache_mutex;
    std::unordered_map<std::string, std::unique_ptr<llvm::TargetMachine>> tm_cache;
}

/**
 * @brief initialize_llvm_targets
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
    std::optional<llvm::Reloc::Model> reloc_model,
    std::optional<llvm::CodeModel::Model> code_model,
    llvm::TargetMachine** out_tm) noexcept
{
    if (!triple_str || !out_tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for triple_str or out_tm");
    }

    std::string cpu_str = cpu ? cpu : "generic";
    std::string feat_str = features ? features : "";
    std::string cache_key = std::string(triple_str) + "|" + cpu_str + "|" + feat_str + "|" +
                            std::to_string(static_cast<int>(opt_level)) + "|" +
                            std::to_string(reloc_model ? static_cast<int>(*reloc_model) : -1) + "|" +
                            std::to_string(code_model ? static_cast<int>(*code_model) : -1);

    std::lock_guard<std::mutex> lock(tm_cache_mutex);
    auto it = tm_cache.find(cache_key);
    if (it != tm_cache.end()) {
        *out_tm = it->second.get();
        return llvm_gen_be_error_t::ok;
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
        cpu_str,
        feat_str,
        opt,
        reloc_model,
        code_model,
        opt_level);

    if (!tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::out_of_memory, __FILE__, __LINE__, 0, "Failed to allocate TargetMachine");
    }

    tm_cache[cache_key] = std::unique_ptr<llvm::TargetMachine>(tm);
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

llvm_gen_be_error_t emit_object_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    const char* output_file_path) noexcept
{
    return emit_machine_code_to_file(module, tm, codegen_file_type_t::object_file, output_file_path);
}

llvm_gen_be_error_t emit_assembly_file(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    const char* output_file_path) noexcept
{
    return emit_machine_code_to_file(module, tm, codegen_file_type_t::assembly_file, output_file_path);
}

llvm_gen_be_error_t emit_machine_code_to_memory_buffer(
    llvm::Module* module,
    llvm::TargetMachine* tm,
    codegen_file_type_t file_type,
    llvm::SmallVectorImpl<char>& out_buffer) noexcept
{
    if (!module || !tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for module or tm");
    }

    llvm::CodeGenFileType cg_file_type;
    switch (file_type) {
        case codegen_file_type_t::assembly_file:
            cg_file_type = llvm::CodeGenFileType::AssemblyFile;
            break;
        case codegen_file_type_t::object_file:
            cg_file_type = llvm::CodeGenFileType::ObjectFile;
            break;
        default:
            return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Invalid file_type for memory buffer emission");
    }

    llvm::raw_svector_ostream dest(out_buffer);
    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr, cg_file_type)) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::code_gen_failure, __FILE__, __LINE__, 0, "Target machine can't emit a memory buffer of this type");
    }

    pass.run(*module);
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t emit_thinlto_summary(
    llvm::Module* module,
    const char* output_file_path) noexcept
{
    if (!module || !output_file_path) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for module or output_file_path");
    }

    std::error_code ec;
    llvm::raw_fd_ostream dest(output_file_path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::io_error, __FILE__, __LINE__, 0, "Could not open output file: %s", ec.message().c_str());
    }

    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::ModulePassManager MPM;
    MPM.addPass(llvm::ThinLTOBitcodeWriterPass(dest, nullptr));
    MPM.run(*module, MAM);

    if (dest.has_error()) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::io_error, __FILE__, __LINE__, 0, "Failed to write ThinLTO index to file");
    }

    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t enable_split_dwarf_emission(
    llvm::TargetMachine* tm,
    const char* dwo_output_path) noexcept
{
    if (!tm || !dwo_output_path) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for tm or dwo_output_path");
    }
    tm->Options.MCOptions.SplitDwarfFile = dwo_output_path;
    return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
