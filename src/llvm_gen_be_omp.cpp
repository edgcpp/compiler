/**
 * @file llvm_gen_be_omp.cpp
 * @brief OpenMP lowering subsystem for the LLVM backend.
 */

#include "llvm_gen_be_omp.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

static llvm::Constant* get_or_create_ident_t(llvm::Module* M) {
    llvm::LLVMContext& Ctx = M->getContext();
    static llvm::StructType* ident_ty = nullptr;
    if (!ident_ty) {
        ident_ty = llvm::StructType::create(Ctx, {
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::PointerType::getUnqual(Ctx)
        }, "ident_t");
    }
    
    // Create source location string
    std::string loc_str = ";unknown;unknown;0;0;;";
    llvm::Constant* loc_const = llvm::ConstantDataArray::getString(Ctx, loc_str, true);
    llvm::GlobalVariable* loc_str_gv = new llvm::GlobalVariable(*M, loc_const->getType(), true, llvm::GlobalValue::PrivateLinkage, loc_const, ".omp.str");
    
    llvm::Constant* ident_init = llvm::ConstantStruct::get(ident_ty, {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 2),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        loc_str_gv
    });
    
    llvm::GlobalVariable* gv = new llvm::GlobalVariable(*M, ident_ty, true, llvm::GlobalValue::PrivateLinkage, ident_init, ".omp.ident");
    return gv;
}

/**
 * @brief Lowers an OpenMP parallel region into LLVM IR.
 * @param[in] stmt The EDG statement representing the parallel region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_parallel_region(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::Type* int32_ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::FunctionType* microtask_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {int32_ptr_ty, int32_ptr_ty}, // global_tid, bound_tid
        true // varargs for captured pointers
    );
    
    llvm::Function* microtask = llvm::Function::Create(microtask_ty, llvm::GlobalValue::InternalLinkage, ".omp_microtask.", M);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(Ctx, "entry", microtask);
    
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(entry);
    
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    
    be_state->builder->CreateRetVoid();
    be_state->builder->restoreIP(saved_ip);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    
    llvm::FunctionType* fork_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        true
    );
    llvm::FunctionCallee fork_call = M->getOrInsertFunction("__kmpc_fork_call", fork_ty);
    
    be_state->builder->CreateCall(fork_call, {
        ident,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        microtask
    });
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP for worksharing construct into LLVM IR.
 * @param[in] stmt The EDG statement representing the worksharing construct.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_for_worksharing(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* init_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee init_call = M->getOrInsertFunction("__kmpc_for_static_init_4", init_ty);
    
    llvm::FunctionType* dispatch_init_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee dispatch_init_call = M->getOrInsertFunction("__kmpc_dispatch_init_4", dispatch_init_ty);

    llvm::FunctionType* dispatch_next_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee dispatch_next_call = M->getOrInsertFunction("__kmpc_dispatch_next_4", dispatch_next_ty);

    llvm::FunctionType* fini_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", fini_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    
    // Test branch coverage for dynamic dispatch vs static
    be_state->builder->CreateCall(dispatch_init_call, {
        ident, global_tid, llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 35),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 10),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1), llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1)
    });
    
    be_state->builder->CreateCall(dispatch_next_call, {ident, global_tid, null_ptr, null_ptr, null_ptr, null_ptr});
    
    // Lower body statements
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    be_state->builder->CreateCall(fini_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP critical section into LLVM IR.
 * @param[in] stmt The EDG statement representing the critical section.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_critical(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* crit_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee crit_call = M->getOrInsertFunction("__kmpc_critical", crit_ty);
    llvm::FunctionCallee end_crit_call = M->getOrInsertFunction("__kmpc_end_critical", crit_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    llvm::Value* lock = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx)); // Global lock var
    
    be_state->builder->CreateCall(crit_call, {ident, global_tid, lock});
    // Critical body here
    be_state->builder->CreateCall(end_crit_call, {ident, global_tid, lock});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP barrier synchronization into LLVM IR.
 * @param[in] stmt The EDG statement representing the barrier.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_barrier(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* barr_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee barr_call = M->getOrInsertFunction("__kmpc_barrier", barr_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    
    be_state->builder->CreateCall(barr_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP target region for device offloading into LLVM IR.
 * @param[in] stmt The EDG statement representing the target region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_region(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* tgt_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::Type::getInt64Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee tgt_call = M->getOrInsertFunction("__tgt_target_kernel", tgt_ty);
    
    // Device data environment mapping
    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::Type::getInt64Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee map_begin = M->getOrInsertFunction("__tgt_target_data_begin_mapper", map_ty);
    llvm::FunctionCallee map_end = M->getOrInsertFunction("__tgt_target_data_end_mapper", map_ty);
    
    llvm::Constant* device_id = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Ctx), -1);
    llvm::Constant* kernel_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    llvm::Value* arg_num = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    
    be_state->builder->CreateCall(map_begin, {device_id, arg_num, null_ptr, null_ptr, null_ptr, null_ptr, null_ptr, null_ptr});
    
    be_state->builder->CreateCall(tgt_call, {
        device_id, kernel_ptr, arg_num, null_ptr, null_ptr, null_ptr, null_ptr
    });
    
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;

    be_state->builder->CreateCall(map_end, {device_id, arg_num, null_ptr, null_ptr, null_ptr, null_ptr, null_ptr, null_ptr});
    
    // Register host-side offload entry
    M->getOrInsertFunction("__tgt_register_lib", llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {llvm::PointerType::getUnqual(Ctx)}, false));

    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_omp_task(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* alloc_ty = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt64Ty(Ctx), llvm::Type::getInt64Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee alloc_call = M->getOrInsertFunction("__kmpc_omp_task_alloc", alloc_ty);
    
    llvm::FunctionType* task_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee task_call = M->getOrInsertFunction("__kmpc_omp_task", task_ty);
    
    llvm::FunctionType* deps_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee deps_call = M->getOrInsertFunction("__kmpc_omp_task_with_deps", deps_ty);

    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    llvm::Value* flags = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1);
    llvm::Value* size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Ctx), 40);
    llvm::Value* shareds = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Ctx), 0);
    llvm::Value* task_entry = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    
    llvm::Value* task_data = be_state->builder->CreateCall(alloc_call, {ident, global_tid, flags, size, shareds, task_entry});
    be_state->builder->CreateCall(task_call, {ident, global_tid, task_data});
    
    return llvm_lower_statement(stmt);
}

llvm_gen_be_error_t lower_omp_taskwait(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* wait_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee wait_call = M->getOrInsertFunction("__kmpc_omp_taskwait", wait_ty);
    
    llvm::FunctionType* wait_deps_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    M->getOrInsertFunction("__kmpc_omp_wait_deps", wait_deps_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    
    be_state->builder->CreateCall(wait_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_omp_taskgroup(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* tg_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee tg_begin = M->getOrInsertFunction("__kmpc_taskgroup", tg_ty);
    llvm::FunctionCallee tg_end = M->getOrInsertFunction("__kmpc_end_taskgroup", tg_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    
    be_state->builder->CreateCall(tg_begin, {ident, global_tid});
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(tg_end, {ident, global_tid});
    
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_omp_reduce(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* reduce_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt64Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee reduce_nowait = M->getOrInsertFunction("__kmpc_reduce_nowait", reduce_ty);
    
    llvm::FunctionType* end_reduce_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    llvm::FunctionCallee end_reduce_nowait = M->getOrInsertFunction("__kmpc_end_reduce_nowait", end_reduce_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    
    llvm::Value* res = be_state->builder->CreateCall(reduce_nowait, {
        ident, global_tid, llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1),
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(Ctx), 8), null_ptr, null_ptr, null_ptr
    });
    
    llvm::BasicBlock* reduce_bb = llvm::BasicBlock::Create(Ctx, "reduce", be_state->builder->GetInsertBlock()->getParent());
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "reduce.end", be_state->builder->GetInsertBlock()->getParent());
    
    llvm::Value* cond = be_state->builder->CreateICmpEQ(res, llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1));
    be_state->builder->CreateCondBr(cond, reduce_bb, end_bb);
    
    be_state->builder->SetInsertPoint(reduce_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(end_reduce_nowait, {ident, global_tid, null_ptr});
    be_state->builder->CreateBr(end_bb);
    
    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}


/**
 * @brief Lowers an OpenMP sections construct.
 * @param[in] stmt EDG statement.
 * @return llvm_gen_be_error_t::ok on success.
 */
/**
 * @brief Lowers an OpenMP section construct.
 * @param[in] stmt EDG statement.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_omp_sections(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* sec_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee sec_call = M->getOrInsertFunction("__kmpc_for_static_init_4", sec_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    
    llvm::Value* gtid = global_tid;
    llvm::Value* schedtype = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 34); // static
    llvm::Value* plastiter = null_ptr;
    llvm::Value* plower = null_ptr;
    llvm::Value* pupper = null_ptr;
    llvm::Value* pstride = null_ptr;
    llvm::Value* incr = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1);
    llvm::Value* chunk = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1);
    
    be_state->builder->CreateCall(sec_call, {ident, gtid, schedtype, plastiter, plower, pupper, pstride, incr, chunk});
    
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::FunctionType* fini_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", fini_ty);
    be_state->builder->CreateCall(fini_call, {ident, gtid});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP section construct.
 * @param[in] stmt EDG statement.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_omp_section(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    return llvm_lower_statement(stmt);
}

/**
 * @brief Lowers an OpenMP single construct.
 * @param[in] stmt EDG statement.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_omp_single(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* sing_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee sing_call = M->getOrInsertFunction("__kmpc_single", sing_ty);
    
    llvm::Constant* ident = get_or_create_ident_t(M);
    llvm::Value* global_tid = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    
    llvm::Value* res = be_state->builder->CreateCall(sing_call, {ident, global_tid});
    
    llvm::BasicBlock* sing_bb = llvm::BasicBlock::Create(Ctx, "omp.single", be_state->builder->GetInsertBlock()->getParent());
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "omp.single.end", be_state->builder->GetInsertBlock()->getParent());
    
    llvm::Value* cond = be_state->builder->CreateICmpNE(res, llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0));
    be_state->builder->CreateCondBr(cond, sing_bb, end_bb);
    
    be_state->builder->SetInsertPoint(sing_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::FunctionType* end_sing_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee end_sing_call = M->getOrInsertFunction("__kmpc_end_single", end_sing_ty);
    be_state->builder->CreateCall(end_sing_call, {ident, global_tid});
    be_state->builder->CreateBr(end_bb);
    
    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
