#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/MDBuilder.h>
#include <string>
#include <iostream>

int main() {
    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("test_module", context);
    
    llvm::Function* f = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context), false), llvm::Function::ExternalLinkage, "test_func", module.get());
    llvm::BasicBlock* bb1 = llvm::BasicBlock::Create(context, "entry", f);
    llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(context, "true_bb", f);
    llvm::BasicBlock* bb3 = llvm::BasicBlock::Create(context, "false_bb", f);

    llvm::IRBuilder<> builder(bb1);
    llvm::Value* cond = builder.getTrue();
    llvm::Instruction* br = builder.CreateCondBr(cond, bb2, bb3);

    llvm::MDBuilder MDB(context);
    llvm::MDNode* weights = MDB.createBranchWeights(100, 1);
    br->setMetadata(llvm::LLVMContext::MD_prof, weights);

    int failed = 0;
    // Verify metadata
    llvm::MDNode* prof_md = br->getMetadata(llvm::LLVMContext::MD_prof);
    if (!prof_md) {
        std::cerr << "Metadata not attached\n";
        failed++;
    } else {
        if (prof_md->getNumOperands() != 3) {
            std::cerr << "Metadata doesn't have 3 operands\n";
            failed++;
        }
        auto* md_str = llvm::dyn_cast<llvm::MDString>(prof_md->getOperand(0));
        if (!md_str || md_str->getString() != "branch_weights") {
            std::cerr << "Metadata string is wrong\n";
            failed++;
        }
    }

    if (failed == 0) {
        std::cout << "All LLVM_PGO tests passed!\n";
        return 0;
    } else {
        std::cerr << failed << " LLVM_PGO tests failed!\n";
        return 1;
    }
}
