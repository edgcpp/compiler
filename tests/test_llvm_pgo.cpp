/**
 * @file test_llvm_pgo.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for Profile-Guided Optimization (PGO) branch weight metadata attachment.
 * @details Verifies `MD_prof` `branch_weights` metadata creation and attachment on conditional branches.
 */

#include "llvm_gen_be_error.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/MDBuilder.h>
#include <string>
#include <iostream>

using namespace edg;

/**
 * @brief Verifies PGO `branch_weights` metadata creation and attachment on a conditional branch.
 * @details Attaches `MD_prof` metadata via `llvm::MDBuilder::createBranchWeights` and inspects its operands.
 * @param[out] out_passed Receives true if `branch_weights` metadata is valid.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_passed` is null.
 */
static llvm_gen_be_error_t run_pgo_tests(bool* out_passed) noexcept {
    LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
    *out_passed = false;

    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("test_module", context);

    llvm::Function* f = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(context), false),
        llvm::Function::ExternalLinkage, "test_func", module.get());
    llvm::BasicBlock* bb1 = llvm::BasicBlock::Create(context, "entry", f);
    llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(context, "true_bb", f);
    llvm::BasicBlock* bb3 = llvm::BasicBlock::Create(context, "false_bb", f);

    llvm::IRBuilder<> builder(bb1);
    llvm::Value* cond = builder.getTrue();
    llvm::Instruction* br = builder.CreateCondBr(cond, bb2, bb3);

    llvm::MDBuilder MDB(context);
    llvm::MDNode* weights = MDB.createBranchWeights(100, 1);
    br->setMetadata(llvm::LLVMContext::MD_prof, weights);

    llvm::MDNode* prof_md = br->getMetadata(llvm::LLVMContext::MD_prof);
    if (prof_md && prof_md->getNumOperands() == 3) {
        auto* md_str = llvm::dyn_cast<llvm::MDString>(prof_md->getOperand(0));
        if (md_str && md_str->getString() == "branch_weights") {
            *out_passed = true;
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the LLVM PGO unit test executable.
 * @details Constructs a conditional branch with `branch_weights` metadata and validates its operands.
 * @return 0 on success, or 1 if any check fails.
 */
int main(void) {
    bool passed = false;
    llvm_gen_be_error_t err_null = run_pgo_tests(nullptr);
    llvm_gen_be_error_t err_ok = run_pgo_tests(&passed);
    if (err_null == llvm_gen_be_error_t::invalid_argument &&
        err_ok == llvm_gen_be_error_t::ok && passed) {
        std::cout << "All LLVM_PGO tests passed!\n";
        return 0;
    }
    return 1;
}
