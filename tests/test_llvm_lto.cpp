/**
 * @file test_llvm_lto.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for ThinLTO and FullLTO module flag and summary index generation.
 * @details Verifies `ThinLTO`, `EnableSplitLTOUnit`, and `FullLTO` module metadata flags and `ModuleSummaryIndex` construction.
 */

#include "llvm_gen_be_error.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <iostream>

using namespace edg;

/**
 * @brief Verifies ThinLTO and FullLTO module metadata flag and summary index construction.
 * @details Tests `ThinLTO`, `EnableSplitLTOUnit`, `FullLTO`, and `ModuleSummaryIndexAnalysis`.
 * @param[out] out_passed Receives true if all LTO checks pass.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_passed` is null.
 */
static llvm_gen_be_error_t run_lto_tests(bool* out_passed) noexcept {
    LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
    *out_passed = false;

    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("test_module", context);

    module->addModuleFlag(llvm::Module::Error, "ThinLTO", 1);
    module->addModuleFlag(llvm::Module::Error, "EnableSplitLTOUnit", 1);

    auto* md = module->getModuleFlag("ThinLTO");
    auto* md_split = module->getModuleFlag("EnableSplitLTOUnit");

    llvm::Function* f = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(context), false),
        llvm::Function::ExternalLinkage, "test_func", module.get());
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", f);
    llvm::IRBuilder<> builder(bb);
    builder.CreateRetVoid();

    llvm::PassBuilder pb;
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    llvm::ModuleSummaryIndex& index = mam.getResult<llvm::ModuleSummaryIndexAnalysis>(*module);
    (void)index;

    auto full_module = std::make_unique<llvm::Module>("test_full_lto", context);
    full_module->addModuleFlag(llvm::Module::Error, "FullLTO", 1);
    auto* md_full = full_module->getModuleFlag("FullLTO");

    *out_passed = (md != nullptr && md_split != nullptr && md_full != nullptr);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the LLVM LTO unit test executable.
 * @details Tests ThinLTO module summary index creation and FullLTO module flag emission.
 * @return 0 on success, or 1 if any check fails.
 */
int main(void) {
    bool passed = false;
    llvm_gen_be_error_t err_null = run_lto_tests(nullptr);
    llvm_gen_be_error_t err_ok = run_lto_tests(&passed);
    if (err_null == llvm_gen_be_error_t::invalid_argument &&
        err_ok == llvm_gen_be_error_t::ok && passed) {
        std::cout << "All LLVM_LTO tests passed!\n";
        return 0;
    }
    return 1;
}