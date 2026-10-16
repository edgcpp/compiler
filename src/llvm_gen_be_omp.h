/**
 * @file llvm_gen_be_omp.h
 * @brief OpenMP lowering subsystem for the LLVM backend.
 */

#ifndef LLVM_GEN_BE_OMP_H
#define LLVM_GEN_BE_OMP_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Function.h>

BEGIN_EDG_NAMESPACE

llvm_gen_be_error_t lower_omp_parallel_region(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_for_worksharing(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_critical(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_barrier(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_target_region(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_task(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_taskwait(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_taskgroup(a_statement_ptr stmt) noexcept;
llvm_gen_be_error_t lower_omp_reduce(a_statement_ptr stmt) noexcept;


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
llvm_gen_be_error_t lower_omp_sections(a_statement_ptr stmt) noexcept;
/**
 * @brief Lowers an OpenMP section construct.
 * @param[in] stmt EDG statement.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_omp_section(a_statement_ptr stmt) noexcept;
/**
 * @brief Lowers an OpenMP single construct.
 * @param[in] stmt EDG statement.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_omp_single(a_statement_ptr stmt) noexcept;

END_EDG_NAMESPACE

#endif
