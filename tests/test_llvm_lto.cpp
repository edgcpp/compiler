#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <llvm/Analysis/ProfileSummaryInfo.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <iostream>

int main() {
    int failed = 0;

    // Test ThinLTO
    {
        llvm::LLVMContext context;
        auto module = std::make_unique<llvm::Module>("test_module", context);
        
        module->addModuleFlag(llvm::Module::Error, "ThinLTO", 1);
        module->addModuleFlag(llvm::Module::Error, "EnableSplitLTOUnit", 1);

        auto* md = module->getModuleFlag("ThinLTO");
        if (!md) {
            std::cerr << "ThinLTO flag missing!\n";
            failed++;
        }
        
        auto* md_split = module->getModuleFlag("EnableSplitLTOUnit");
        if (!md_split) {
            std::cerr << "EnableSplitLTOUnit flag missing!\n";
            failed++;
        }

        llvm::Function* f = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context), false), llvm::Function::ExternalLinkage, "test_func", module.get());
        llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", f);
        llvm::IRBuilder<> builder(bb);
        builder.CreateRetVoid();

        llvm::ProfileSummaryInfo PSI(*module);
        auto GetBFI = [](const llvm::Function &F) -> llvm::BlockFrequencyInfo* { return nullptr; };
        llvm::ModuleSummaryIndex index = llvm::buildModuleSummaryIndex(*module, GetBFI, &PSI);
        


        std::string out;
        llvm::raw_string_ostream os(out);
        // Writing bitcode with index shouldn't crash
        // llvm::WriteBitcodeToFile(*module, os, false, &index, false);
        // if (out.empty()) failed++;
    }

    // Test FullLTO
    {
        llvm::LLVMContext context;
        auto module = std::make_unique<llvm::Module>("test_module", context);
        
        module->addModuleFlag(llvm::Module::Error, "FullLTO", 1);

        auto* md = module->getModuleFlag("FullLTO");
        if (!md) {
            std::cerr << "FullLTO flag missing!\n";
            failed++;
        }
    }

    if (failed == 0) {
        std::cout << "All LLVM_LTO tests passed!\n";
        return 0;
    } else {
        std::cerr << failed << " LLVM_LTO tests failed!\n";
        return 1;
    }
}