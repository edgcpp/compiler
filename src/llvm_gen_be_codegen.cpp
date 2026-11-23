/**
 * @file llvm_gen_be_codegen.cpp
 * @ingroup llvm_gen_be_driver
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

#if BACK_END_IS_LLVM_GEN_BE

BEGIN_EDG_NAMESPACE

namespace {
    std::mutex tm_cache_mutex; ///< Mutex guarding concurrent access to `tm_cache`.
    std::unordered_map<std::string, std::unique_ptr<llvm::TargetMachine>> tm_cache; ///< Cache of instantiated `TargetMachine` objects keyed by target configuration.
}

/**
 * @brief Initializes all LLVM target info, target machine, MC, asm printer, and asm parser registries.
 * @details Must be called before looking up targets or creating `TargetMachine` instances.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t initialize_llvm_targets(void) noexcept {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
    return llvm_gen_be_error_t::ok;
}

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
    llvm::TargetMachine** out_tm) noexcept
{
    if (!triple_str || !out_tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for triple_str or out_tm");
    }

    bool fn_sec = (be_state != nullptr && be_state->function_sections);
    bool data_sec = (be_state != nullptr && be_state->data_sections);
    std::string cpu_str = cpu ? cpu : "generic";
    std::string feat_str = features ? features : "";
    std::string cache_key = std::string(triple_str) + "|" + cpu_str + "|" + feat_str + "|" +
                            std::to_string(static_cast<int>(opt_level)) + "|" +
                            std::to_string(reloc_model ? static_cast<int>(*reloc_model) : -1) + "|" +
                            std::to_string(code_model ? static_cast<int>(*code_model) : -1) + "|" +
                            (fn_sec ? "1" : "0") + "|" + (data_sec ? "1" : "0");

    std::lock_guard<std::mutex> lock(tm_cache_mutex);
    auto it = tm_cache.find(cache_key);
    if (it != tm_cache.end()) {
        *out_tm = it->second.get();
        return llvm_gen_be_error_t::ok;
    }

    std::string error_str;
    llvm::Triple theTriple = llvm::Triple(triple_str);
#if LLVM_VERSION_MAJOR >= 21
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(theTriple, error_str);
#else
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(theTriple.getTriple(), error_str);
#endif
    if (!target) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Failed to lookup target: %s", error_str.c_str());
    }

    llvm::TargetOptions opt;
    opt.FunctionSections = fn_sec;
    opt.DataSections = data_sec;

    llvm::TargetMachine* tm = target->createTargetMachine(
#if LLVM_VERSION_MAJOR >= 21
        theTriple,
#else
        theTriple.getTriple(),
#endif
        cpu_str,
        feat_str,
        opt,
        reloc_model,
        code_model,
        opt_level);

    tm_cache[cache_key] = std::unique_ptr<llvm::TargetMachine>(tm);
    *out_tm = tm;
    return llvm_gen_be_error_t::ok;
}

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

    llvm::raw_fd_ostream dest{output_file_path, ec, open_flags};
    if (ec) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::io_error, __FILE__, __LINE__, 0, "Could not open output file: %s", ec.message().c_str());
    }

    if (file_type == codegen_file_type_t::llvm_ir_text) {
        module->print(dest, nullptr);
        return llvm_gen_be_error_t::ok;
    }

    if (file_type == codegen_file_type_t::bitcode_file) {
        llvm::WriteBitcodeToFile(*module, dest);
        return llvm_gen_be_error_t::ok;
    }

    if (!tm) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "TargetMachine is required for object and assembly emission");
    }

    llvm::CodeGenFileType cg_file_type;
    switch (file_type) {
        case codegen_file_type_t::assembly_file:
#if LLVM_VERSION_MAJOR >= 18
            cg_file_type = llvm::CodeGenFileType::AssemblyFile;
#else
            cg_file_type = llvm::CGFT_AssemblyFile;
#endif
            break;
        case codegen_file_type_t::object_file:
#if LLVM_VERSION_MAJOR >= 18
            cg_file_type = llvm::CodeGenFileType::ObjectFile;
#else
            cg_file_type = llvm::CGFT_ObjectFile;
#endif
            break;
        default:
            return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Unknown codegen_file_type_t");
    }

    llvm::legacy::PassManager pass;
    std::unique_ptr<llvm::raw_fd_ostream> dwo_dest;
    if (!be_state->split_dwarf_file.empty()) {
        llvm::raw_fd_ostream* raw_dwo = nullptr;
        LLVM_GEN_BE_NEW(raw_dwo, llvm::raw_fd_ostream(
            be_state->split_dwarf_file, ec, llvm::sys::fs::OF_None));
        dwo_dest.reset(raw_dwo);
        if (ec) {
            return llvm_gen_be_set_error(
                &be_state->err_context, llvm_gen_be_error_t::io_error,
                __FILE__, __LINE__, 0, "Could not open DWO output file: %s", ec.message().c_str());
        }
    }
    tm->addPassesToEmitFile(pass, dest, dwo_dest.get(), cg_file_type);
    pass.run(*module);
    dest.flush();
    if (dwo_dest) {
        dwo_dest->flush();
    }

    return llvm_gen_be_error_t::ok;
}

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
    const char* output_file_path) noexcept
{
    return emit_machine_code_to_file(module, tm, codegen_file_type_t::object_file, output_file_path);
}

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
    const char* output_file_path) noexcept
{
    return emit_machine_code_to_file(module, tm, codegen_file_type_t::assembly_file, output_file_path);
}

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
    llvm::SmallVectorImpl<char>* out_buffer) noexcept
{
    if (!module || !tm || !out_buffer) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for module, tm, or out_buffer");
    }

    llvm::CodeGenFileType cg_file_type;
    switch (file_type) {
        case codegen_file_type_t::assembly_file:
#if LLVM_VERSION_MAJOR >= 18
            cg_file_type = llvm::CodeGenFileType::AssemblyFile;
#else
            cg_file_type = llvm::CGFT_AssemblyFile;
#endif
            break;
        case codegen_file_type_t::object_file:
#if LLVM_VERSION_MAJOR >= 18
            cg_file_type = llvm::CodeGenFileType::ObjectFile;
#else
            cg_file_type = llvm::CGFT_ObjectFile;
#endif
            break;
        default:
            return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Invalid file_type for memory buffer emission");
    }

    llvm::raw_svector_ostream dest{*out_buffer};

    llvm::legacy::PassManager pass;
    tm->addPassesToEmitFile(pass, dest, nullptr, cg_file_type);
    pass.run(*module);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Clears and releases all cached TargetMachine instances.
 * @details Safely destroys all `TargetMachine` objects stored in the internal cache.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t release_target_machine_cache(void) noexcept
{
    std::lock_guard<std::mutex> lock(tm_cache_mutex);
    tm_cache.clear();
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a ThinLTO bitcode file containing module summary index records.
 * @details Runs `ThinLTOBitcodeWriterPass` on `module` and writes the output to `output_file_path`.
 * @param[in] module The LLVM module to summarize and write.
 * @param[in] output_file_path The destination ThinLTO bitcode file path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t emit_thinlto_summary(
    llvm::Module* module,
    const char* output_file_path) noexcept
{
    if (!module || !output_file_path) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::invalid_argument, __FILE__, __LINE__, 0, "Null pointer provided for module or output_file_path");
    }

    std::error_code ec;
    llvm::raw_fd_ostream dest{output_file_path, ec, llvm::sys::fs::OF_None};
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

    return llvm_gen_be_error_t::ok;
}

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
    bool thin_lto) noexcept {
    if (thin_lto) {
        return emit_thinlto_summary(module, output_path);
    }
    return emit_machine_code_to_file(module, nullptr, codegen_file_type_t::bitcode_file, output_path);
}

/**
 * @brief Configures Split DWARF (`.dwo`) emission on the given TargetMachine.
 * @details Sets `MCOptions.SplitDwarfFile` to `dwo_output_path`.
 * @param[in,out] tm The TargetMachine to configure.
 * @param[in] dwo_output_path Path to the `.dwo` output file.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
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

#endif /* BACK_END_IS_LLVM_GEN_BE */
