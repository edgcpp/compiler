/**
 * @file llvm_gen_be_driver.h
 * @ingroup llvm_gen_be_driver
 * @brief Toolchain detection and linker driver orchestrator.
 * @details Declares functions for invoking the system linker and performing in-process
 *          LTO optimization and object emission for the EDG LLVM backend.
 */
#ifndef LLVM_GEN_BE_DRIVER_H
#define LLVM_GEN_BE_DRIVER_H

#include "llvm_gen_be_internal.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @struct llvm_toolchain_config_t
 * @brief Holds resolved native toolchain, SDK/sysroot, CRT object, and library search paths.
 * @details Populated by `discover_target_toolchain` and consumed by `build_native_linker_args`
 *          when invoking `lld` (`ld.lld`, `ld64.lld`, `lld-link`), system `ld`, or `clang++`.
 */
struct llvm_toolchain_config_t {
  std::string linker_program;             ///< Resolved path to the linker driver executable.
  std::string sysroot_path;               ///< Resolved SDK or sysroot directory path (`--sysroot` or `-syslibroot`).
  std::string crt1_path;                  ///< Resolved path to `crt1.o` / `Scrt1.o` startup object (ELF).
  std::string crti_path;                  ///< Resolved path to `crti.o` prologue object (ELF).
  std::string crtbegin_path;              ///< Resolved path to `crtbegin.o` / `crtbeginS.o` (ELF).
  std::string crtend_path;                ///< Resolved path to `crtend.o` / `crtendS.o` (ELF).
  std::string crtn_path;                  ///< Resolved path to `crtn.o` epilogue object (ELF).
  std::string dynamic_linker_path;        ///< Resolved ELF dynamic linker path (`-dynamic-linker`).
  std::string macos_deployment_version;   ///< Resolved macOS deployment version (`MACOSX_DEPLOYMENT_TARGET` / host).
  std::string cxx_stdlib_flag;            ///< Standard C++ library flag (`-lc++`, `-lstdc++`, or `msvcprt.lib`).
  std::string unwind_lib_flag;            ///< Unwind/ABI library flag (`-lunwind`, `-lgcc_s`, or `-lc++abi`).
  std::vector<std::string> lib_search_dirs; ///< Discovered library search directories (`-L` / `/LIBPATH:`).
  bool is_direct_lld = false;             ///< `true` when invoking `ld.lld` / `ld64.lld` / `lld-link` directly instead of a compiler driver.
};

/**
 * @brief Resolves the effective LLVM relocation model for a target triple and backend state.
 * @details Honors an explicit `be_state->reloc_model` override when present, selects
 *          `llvm::Reloc::Static` for static non-shared links (`-static`), defaults hosted
 *          ELF targets (e.g., Linux, BSD, Android) as well as Mach-O and 64-bit Windows COFF
 *          targets to `llvm::Reloc::PIC_` so emitted objects link cleanly with PIE-by-default
 *          system toolchains, and falls back to `llvm::Reloc::Static` for bare-metal ELF,
 *          32-bit COFF, and WebAssembly modules.
 * @param[in] triple Target LLVM triple being lowered or emitted.
 * @param[out] out_reloc Non-null pointer receiving the resolved `llvm::Reloc::Model`.
 * @return `llvm_gen_be_error_t::ok` on success, `invalid_argument` if @p out_reloc is null,
 *         or `out_of_memory` on allocation budget exhaustion.
 */
llvm_gen_be_error_t llvm_resolve_effective_reloc_model(
    const llvm::Triple& triple,
    llvm::Reloc::Model* out_reloc) noexcept;

/**
 * @brief Probes the host/target environment to discover SDK, CRT, and library paths for @p triple.
 * @details Inspects Linux multiarch/GCC directories, macOS CommandLineTools/Xcode SDKs, or
 *          Windows MSVC/WindowsSDK directories, honoring @p sysroot_override when non-null.
 * @param[in] triple Target LLVM triple being linked.
 * @param[in] sysroot_override Optional explicit sysroot/SDK path override (may be null).
 * @param[out] out_config Receives the populated `llvm_toolchain_config_t`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_config` is null.
 */
llvm_gen_be_error_t discover_target_toolchain(
    const llvm::Triple& triple,
    const char* sysroot_override,
    llvm_toolchain_config_t* out_config) noexcept;

/**
 * @brief Synthesizes the complete command-line argument vector for linking @p obj_files into @p exe_file_path.
 * @details Formats target triple, sysroot, CRT objects, library search paths, sanitizer/profile/OpenMP
 *          runtimes, and user `extra_linker_args` for ELF, Mach-O, or COFF targets.
 * @param[in] tc Discovered toolchain configuration.
 * @param[in] triple Target LLVM triple.
 * @param[in] obj_files Non-empty list of input object or bitcode file paths.
 * @param[in] exe_file_path Desired output executable path.
 * @param[out] out_args Receives the constructed command-line argument strings.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are invalid.
 */
llvm_gen_be_error_t build_native_linker_args(
    const llvm_toolchain_config_t& tc,
    const llvm::Triple& triple,
    const std::vector<std::string>& obj_files,
    const char* exe_file_path,
    std::vector<std::string>* out_args) noexcept;

/**
 * @brief Invokes the system linker to link multiple object files into an executable.
 * @details Discovers the target toolchain, synthesizes linker flags via `build_native_linker_args`,
 *          and executes the linker via `llvm::sys::ExecuteAndWait`.
 * @param[in] obj_files Vector of object file paths to link.
 * @param[in] exe_file_path The desired path for the final executable.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_link_executables(
    const std::vector<std::string>& obj_files,
    const char* exe_file_path) noexcept;

/**
 * @brief Invokes the system linker to create an executable.
 * @details Detects system toolchains, libraries, and SDKs and executes the driver via `llvm::sys::ExecuteAndWait`.
 * @param[in] obj_file_path The path to the object file to link.
 * @param[in] exe_file_path The desired path for the final executable.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_link_executable(const char* obj_file_path, const char* exe_file_path) noexcept;

/**
 * @brief Performs multi-module ThinLTO/FullLTO combined index generation, IR linking, and native executable emission.
 * @details Parses each bitcode file in @p bitcode_files, builds a combined `ModuleSummaryIndex`,
 *          links the modules into a unified LTO module, runs the LTO optimization pipeline,
 *          emits a native object, and links `@p output_exe_path` when non-null.
 * @param[in] bitcode_files List of LLVM bitcode (`.bc`) file paths to merge and optimize.
 * @param[in] output_exe_path Optional path for the linked executable (if null, only LTO object emission runs).
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t run_thinlto_combined_index_and_link(
    const std::vector<std::string>& bitcode_files,
    const char* output_exe_path) noexcept;

/**
 * @brief Performs direct embedded libLLVM LTO optimization and TargetMachine object emission.
 * @details Runs the LTO optimization pipeline on `be_state->module`, emits a native object
 *          artifact via `TargetMachine`, and invokes `llvm_link_executable` when `ofile_name` is set.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t invoke_embedded_lld_lto(void) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_DRIVER_H */
