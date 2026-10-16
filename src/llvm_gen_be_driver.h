/**
 * @file llvm_gen_be_driver.h
 * @brief Toolchain detection and linker driver orchestrator.
 */
#ifndef LLVM_GEN_BE_DRIVER_H
#define LLVM_GEN_BE_DRIVER_H

#include "llvm_gen_be_internal.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Invokes the system linker (lld, ld64, MSVC link.exe) to create an executable.
 * @details Detects system toolchains, libraries, and SDKs to properly link the
 * generated object file into a runnable executable. Supports LTO configurations.
 * @param[in] obj_file_path The path to the object file to link.
 * @param[in] exe_file_path The desired path for the final executable.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_link_executable(const char* obj_file_path, const char* exe_file_path) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_DRIVER_H */
