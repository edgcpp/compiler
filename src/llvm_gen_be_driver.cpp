/**
 * @file llvm_gen_be_driver.cpp
 * @ingroup llvm_gen_be_driver
 * @brief Toolchain detection and linker driver orchestrator.
 * @details Implements the invocation of system linkers (LLD, GNU ld, Apple ld64, MSVC link.exe)
 *          along with resolution of standard libraries (libc++, libstdc++, MSVCRT) and
 *          multi-module ThinLTO/FullLTO combined index and IR linking pipelines.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_driver.h"
#include "llvm_gen_be_codegen.h"
#include "llvm_gen_be_opt.h"
#include "cmd_line.h"
#include <cstdlib>
#include <string>
#include <vector>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/ModuleSummaryIndex.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Normalizes a macOS version string (`"15"`, `"15.2"`, or `"15.2.1"`) into `major.minor.patch`.
 * @details Appends `.0.0` or `.0` as needed so `ld64.lld -platform_version macos` receives a 3-component version.
 * @param[in] raw Raw macOS version string.
 * @param[out] out_version Receives normalized `major.minor.patch` string.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t normalize_macos_version_string(
    llvm::StringRef raw,
    std::string* out_version) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    std::string v = raw.str();
    size_t first_dot = v.find('.');
    if (first_dot == std::string::npos) {
        v += ".0.0";
    } else if (v.find('.', first_dot + 1) == std::string::npos) {
        v += ".0";
    }
    *out_version = v;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Detects the macOS deployment target version string for `ld64.lld -platform_version`.
 * @details Checks @p triple OS version components first, then `MACOSX_DEPLOYMENT_TARGET`,
 *          then `sysctlbyname("kern.osproductversion")` on Apple hosts, falling back to `"14.0.0"`.
 * @param[in] triple Target Darwin triple.
 * @param[out] out_version Receives normalized `"major.minor.patch"` version string.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t detect_macos_deployment_version(
    const llvm::Triple& triple,
    std::string* out_version) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (triple.getOSMajorVersion() > 0) {
        llvm::VersionTuple vt = triple.getOSVersion();
        unsigned minor_v = vt.getMinor().value_or(0);
        unsigned sub_v = vt.getSubminor().value_or(0);
        *out_version = std::to_string(vt.getMajor()) + "." +
                       std::to_string(minor_v) + "." + std::to_string(sub_v);
        return llvm_gen_be_error_t::ok;
    }
    const char* env_ver = std::getenv("MACOSX_DEPLOYMENT_TARGET");
    if (env_ver && env_ver[0] != '\0') {
        return normalize_macos_version_string(env_ver, out_version);
    }
#if defined(__APPLE__)
    char prod_ver[64] = "14.0.0";
    size_t len = sizeof(prod_ver) - 1;
    sysctlbyname("kern.osproductversion", prod_ver, &len, nullptr, 0);
    return normalize_macos_version_string(prod_ver, out_version);
#else
    *out_version = "14.0.0";
    return llvm_gen_be_error_t::ok;
#endif
}

/**
 * @brief Discovers GCC CRT (`crtbeginS.o`/`crtbegin.o`, `crtendS.o`/`crtendS.o`) and ELF dynamic linker paths.
 * @details Scans `/usr/lib/gcc/<tuple>/<ver>` and `/usr/lib64/gcc/<tuple>/<ver>` directories under
 *          @p root and selects the architecture-appropriate ELF interpreter path.
 * @param[in] triple Target ELF triple.
 * @param[in] root Sysroot prefix (empty for host root).
 * @param[in] use_pic_crt `true` to prefer `Scrt1.o` / `crtbeginS.o` / `crtendS.o` (PIE/PIC/shared).
 * @param[in,out] out_config Toolchain configuration to populate.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t discover_linux_gcc_crt_and_ldso(
    const llvm::Triple& triple,
    const std::string& root,
    bool use_pic_crt,
    llvm_toolchain_config_t* out_config) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    std::string arch = triple.getArchName().str();
    std::string arch_tuple = arch + "-linux-gnu";
    std::string usr_multiarch = root + "/usr/lib/" + arch_tuple;
    std::string lib_multiarch = root + "/lib/" + arch_tuple;
    std::string usr_lib64 = root + "/usr/lib64";
    std::string crt_dir = llvm::sys::fs::exists(usr_multiarch + "/crti.o") ||
                          llvm::sys::fs::exists(usr_multiarch + "/crt1.o")
                              ? usr_multiarch
                              : (llvm::sys::fs::exists(usr_lib64 + "/crti.o")
                                     ? usr_lib64
                                     : (root + "/usr/lib"));

    if (use_pic_crt && llvm::sys::fs::exists(crt_dir + "/Scrt1.o")) {
        out_config->crt1_path = crt_dir + "/Scrt1.o";
    } else {
        out_config->crt1_path = crt_dir + "/crt1.o";
    }
    out_config->crti_path = crt_dir + "/crti.o";
    out_config->crtn_path = crt_dir + "/crtn.o";

    const char* begin_name = use_pic_crt ? "crtbeginS.o" : "crtbegin.o";
    const char* end_name = use_pic_crt ? "crtendS.o" : "crtend.o";
    std::string gcc_crt_dir;
    std::vector<std::string> candidate_gcc_bases = {
        root + "/usr/lib/gcc/" + triple.str(),
        root + "/usr/lib/gcc/" + arch_tuple,
        root + "/usr/lib64/gcc/" + arch_tuple,
        root + "/usr/lib/gcc/" + arch + "-redhat-linux",
        root + "/usr/lib/gcc/" + arch + "-alpine-linux-musl",
        root + "/lib/gcc/" + arch_tuple
    };
    std::string best_ver;
    for (const std::string& base_dir : candidate_gcc_bases) {
        std::error_code ec;
        for (llvm::sys::fs::directory_iterator dir_it(base_dir, ec), dir_end;
             !ec && dir_it != dir_end; dir_it.increment(ec)) {
            std::string ver_dir = dir_it->path();
            if (llvm::sys::fs::exists(ver_dir + "/" + begin_name) ||
                llvm::sys::fs::exists(ver_dir + "/crtbegin.o")) {
                std::string ver_leaf = ver_dir.substr(base_dir.size() + 1);
                if (gcc_crt_dir.empty() || ver_leaf > best_ver) {
                    best_ver = ver_leaf;
                    gcc_crt_dir = ver_dir;
                }
            }
        }
        if (!gcc_crt_dir.empty()) break;
    }

    if (!gcc_crt_dir.empty()) {
        out_config->crtbegin_path = llvm::sys::fs::exists(gcc_crt_dir + "/" + begin_name)
                                        ? (gcc_crt_dir + "/" + begin_name)
                                        : (gcc_crt_dir + "/crtbegin.o");
        out_config->crtend_path = llvm::sys::fs::exists(gcc_crt_dir + "/" + end_name)
                                      ? (gcc_crt_dir + "/" + end_name)
                                      : (gcc_crt_dir + "/crtend.o");
        out_config->lib_search_dirs.push_back(gcc_crt_dir);
    } else {
        out_config->crtbegin_path = crt_dir + "/" + begin_name;
        out_config->crtend_path = crt_dir + "/" + end_name;
    }

    out_config->lib_search_dirs.push_back(usr_multiarch);
    out_config->lib_search_dirs.push_back(lib_multiarch);
    out_config->lib_search_dirs.push_back(usr_lib64);
    out_config->lib_search_dirs.push_back(root + "/usr/lib");
    out_config->lib_search_dirs.push_back(root + "/lib");

    if (triple.isMusl()) {
        out_config->dynamic_linker_path = "/lib/ld-musl-" + arch + ".so.1";
    } else if (triple.getArch() == llvm::Triple::x86_64) {
        out_config->dynamic_linker_path = "/lib64/ld-linux-x86-64.so.2";
    } else if (triple.isAArch64()) {
        out_config->dynamic_linker_path = "/lib/ld-linux-aarch64.so.1";
    } else if (triple.isRISCV64()) {
        out_config->dynamic_linker_path = "/lib/ld-linux-riscv64-lp64d.so.1";
    } else if (triple.isARM() || triple.isThumb()) {
        out_config->dynamic_linker_path = "/lib/ld-linux-armhf.so.3";
    } else {
        out_config->dynamic_linker_path = "/lib/ld-linux.so.2";
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Probes the host/target environment to discover SDK, CRT, and library paths for @p triple.
 * @details Inspects Linux multiarch/GCC directories, macOS CommandLineTools/Xcode SDKs, or
 *          Windows MSVC/WindowsSDK directories, honoring @p sysroot_override when non-null,
 *          and selects `clang`/`cc` for C vs `clang++`/`c++` for C++.
 * @param[in] triple Target LLVM triple being linked.
 * @param[in] sysroot_override Optional explicit sysroot/SDK path override (may be null).
 * @param[out] out_config Receives the populated `llvm_toolchain_config_t`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_config` is null.
 */
llvm_gen_be_error_t discover_target_toolchain(
    const llvm::Triple& triple,
    const char* sysroot_override,
    llvm_toolchain_config_t* out_config) noexcept {
    if (!out_config) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_config = llvm_toolchain_config_t{};

    if (sysroot_override && sysroot_override[0] != '\0') {
        out_config->sysroot_path = sysroot_override;
    } else if (be_state && !be_state->sysroot_override.empty()) {
        out_config->sysroot_path = be_state->sysroot_override;
    }

    const bool is_cxx = (!be_state || be_state->language_is_cplusplus);
    const bool use_pic_crt = (!be_state || !be_state->static_link);

    if (triple.isOSDarwin()) {
        if (out_config->sysroot_path.empty()) {
            const char* sdk_env = std::getenv("SDKROOT");
            if (sdk_env) {
                out_config->sysroot_path = sdk_env;
            } else {
                out_config->sysroot_path = "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk";
            }
        }
        out_config->lib_search_dirs.push_back(out_config->sysroot_path + "/usr/lib");
        if (is_cxx) {
            out_config->cxx_stdlib_flag = "-lc++";
        }
        out_config->unwind_lib_flag = "-lSystem";
        LLVM_GEN_BE_CHECK(detect_macos_deployment_version(triple, &out_config->macos_deployment_version));
    } else if (triple.isOSWindows()) {
        const char* vc_dir = std::getenv("VCToolsInstallDir");
        if (vc_dir) {
            out_config->lib_search_dirs.push_back(std::string(vc_dir) + "/lib/x64");
        }
        const char* win_sdk = std::getenv("WindowsSdkDir");
        if (win_sdk) {
            out_config->lib_search_dirs.push_back(std::string(win_sdk) + "/Lib/um/x64");
        }
        if (is_cxx) {
            out_config->cxx_stdlib_flag = "-lmsvcprt";
        }
        out_config->unwind_lib_flag = "-lvcruntime";
    } else {
        LLVM_GEN_BE_CHECK(discover_linux_gcc_crt_and_ldso(
            triple, out_config->sysroot_path, use_pic_crt, out_config));
        if (is_cxx) {
            out_config->cxx_stdlib_flag = "-lstdc++";
        }
        out_config->unwind_lib_flag = "-lgcc_s";
    }

    const char* primary_driver = is_cxx ? "clang++" : "clang";
    const char* fallback_driver = is_cxx ? "c++" : "cc";
    llvm::ErrorOr<std::string> driver_path = llvm::sys::findProgramByName(primary_driver);
    if (!driver_path) {
        driver_path = llvm::sys::findProgramByName(fallback_driver);
    }
    if (driver_path) {
        out_config->linker_program = *driver_path;
        out_config->is_direct_lld = false;
    } else {
        const char* lld_name = triple.isWasm()
                                   ? "wasm-ld"
                                   : (triple.isOSWindows()
                                          ? "lld-link"
                                          : (triple.isOSDarwin() ? "ld64.lld" : "ld.lld"));
        llvm::ErrorOr<std::string> lld_path = llvm::sys::findProgramByName(lld_name);
        if (lld_path) {
            out_config->linker_program = *lld_path;
            out_config->is_direct_lld = true;
        }
    }

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Appends extra user linker arguments to @p out_args, unpacking `-Wl,a,b` when invoking `lld` directly.
 * @details Preserves `-L<dir>`, `-l<lib>`, `-framework <Name>`, and raw flags, while splitting
 *          comma-delimited `-Wl,...` passthrough tokens into direct linker flags for `ld.lld`/`ld64.lld`.
 * @param[in] extra_args User/driver extra linker flags.
 * @param[in] is_direct_lld `true` when invoking `lld` directly rather than a compiler driver.
 * @param[in,out] out_args Destination command-line vector.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t append_extra_linker_flags(
    const std::vector<std::string>& extra_args,
    bool is_direct_lld,
    std::vector<std::string>* out_args) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    for (const std::string& extra : extra_args) {
        if (is_direct_lld && llvm::StringRef(extra).starts_with("-Wl,")) {
            llvm::StringRef rest = llvm::StringRef(extra).substr(4);
            while (!rest.empty()) {
                size_t comma = rest.find(',');
                llvm::StringRef tok = (comma == llvm::StringRef::npos) ? rest : rest.slice(0, comma);
                if (!tok.empty()) {
                    out_args->push_back(tok.str());
                }
                if (comma == llvm::StringRef::npos) break;
                rest = rest.substr(comma + 1);
            }
        } else {
            out_args->push_back(extra);
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Synthesizes the complete command-line argument vector for linking @p obj_files into @p exe_file_path.
 * @details Formats target triple, sysroot, `-shared`/`-static`/`-pie`/`-no-pie`/`-pthread`/`-rdynamic`,
 *          CRT objects, dynamic linker, library search paths, sanitizer/profile/OpenMP runtimes,
 *          and user `extra_linker_args` (`-L`, `-l`, `-Wl,*`, `-framework`) for ELF, Mach-O, COFF, or Wasm targets.
 * @param[in] tc Discovered toolchain configuration.
 * @param[in] triple Target LLVM triple.
 * @param[in] obj_files Non-empty list of input object or bitcode file paths.
 * @param[in] exe_file_path Desired output executable or shared library path.
 * @param[out] out_args Receives the constructed command-line argument strings.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are invalid.
 */
llvm_gen_be_error_t build_native_linker_args(
    const llvm_toolchain_config_t& tc,
    const llvm::Triple& triple,
    const std::vector<std::string>& obj_files,
    const char* exe_file_path,
    std::vector<std::string>* out_args) noexcept {
    if (tc.linker_program.empty() || obj_files.empty() || !exe_file_path || !out_args) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    out_args->clear();
    out_args->push_back(tc.linker_program);

    const bool is_shared = (be_state && be_state->shared_link);
    const bool is_static = (be_state && be_state->static_link);
    const bool is_pthread = (be_state && be_state->pthread_enabled);
    const bool is_rdynamic = (be_state && be_state->rdynamic_enabled);
    const int pie_mode = be_state ? be_state->pie_mode : 0;

    if (!tc.is_direct_lld) {
        if (!llvm::StringRef(tc.linker_program).ends_with("cc") ||
            llvm::StringRef(tc.linker_program).contains("clang")) {
            out_args->push_back("-target");
            out_args->push_back(triple.str());
        }
        if (!tc.sysroot_path.empty() && !triple.isOSDarwin()) {
            out_args->push_back("--sysroot=" + tc.sysroot_path);
        }
        if (is_shared) out_args->push_back("-shared");
        if (is_static) out_args->push_back("-static");
        if (!is_shared && !is_static) {
            if (pie_mode > 0) out_args->push_back("-pie");
            else if (pie_mode < 0 && !triple.isOSDarwin()) out_args->push_back("-no-pie");
        }
        if (is_pthread) out_args->push_back("-pthread");
        if (is_rdynamic && !triple.isOSDarwin() && !triple.isOSWindows()) {
            out_args->push_back("-rdynamic");
        }
        if (be_state && be_state->nostdlib_link) out_args->push_back("-nostdlib");
        if (be_state && be_state->nodefaultlibs_link) out_args->push_back("-nodefaultlibs");
        if (be_state && be_state->nostartfiles_link) out_args->push_back("-nostartfiles");
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        if (sanitize_address || (be_state && be_state->sanitize_address_opt)) out_args->push_back("-fsanitize=address");
        if (sanitize_thread || (be_state && be_state->sanitize_thread_opt)) out_args->push_back("-fsanitize=thread");
        if (sanitize_memory || (be_state && be_state->sanitize_memory_opt)) out_args->push_back("-fsanitize=memory");
        if (sanitize_undefined || (be_state && be_state->sanitize_undefined)) out_args->push_back("-fsanitize=undefined");
        if (sanitize_hwaddress) out_args->push_back("-fsanitize=hwaddress");
        if (use_safestack) out_args->push_back("-fsanitize=safe-stack");
        if (use_shadowcallstack) out_args->push_back("-fsanitize=shadow-call-stack");
        if (fprofile_generate_path) out_args->push_back("-fprofile-instr-generate");
        if (flto_kind) {
            if (std::string(flto_kind) == "thin") out_args->push_back("-flto=thin");
            else if (std::string(flto_kind) == "full") out_args->push_back("-flto=full");
        } else if (be_state && be_state->lto_mode == edg_lto_mode_t::thin_lto) {
            out_args->push_back("-flto=thin");
        } else if (be_state && be_state->lto_mode == edg_lto_mode_t::full_lto) {
            out_args->push_back("-flto=full");
        }
        if ((be_state && be_state->fopenmp_enabled) ||
            (be_state && be_state->module && be_state->module->getFunction("__kmpc_fork_call") != nullptr)) {
            const char* env_omp = std::getenv("EDG_LIBOMP_LIB_DIR");
            std::string omp_dir = (env_omp && env_omp[0] != '\0')
                                      ? std::string(env_omp)
                                      : std::string("/opt/homebrew/opt/libomp/lib");
            if (triple.isOSDarwin() && llvm::sys::fs::exists(omp_dir + "/libomp.dylib")) {
                out_args->push_back("-L" + omp_dir);
                out_args->push_back("-Wl,-rpath," + omp_dir);
                out_args->push_back("-lomp");
            } else {
                out_args->push_back("-fopenmp");
            }
        }
        if (be_state) {
            LLVM_GEN_BE_CHECK(append_extra_linker_flags(be_state->extra_linker_args, false, out_args));
        }
        out_args->push_back("-o");
        out_args->push_back(exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (triple.isOSWindows()) {
        if (is_shared) {
            out_args->push_back("/dll");
        }
        for (const std::string& dir : tc.lib_search_dirs) {
            out_args->push_back("/libpath:" + dir);
        }
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        out_args->push_back("/defaultlib:libcmt");
        out_args->push_back("/defaultlib:oldnames");
        if (!tc.cxx_stdlib_flag.empty()) {
            out_args->push_back("msvcprt.lib");
        }
        out_args->push_back("vcruntime.lib");
        out_args->push_back("ucrt.lib");
        out_args->push_back("kernel32.lib");
        if (be_state) {
            LLVM_GEN_BE_CHECK(append_extra_linker_flags(be_state->extra_linker_args, true, out_args));
        }
        out_args->push_back(std::string("/out:") + exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (triple.isOSDarwin()) {
        std::string macos_ver = tc.macos_deployment_version;
        if (macos_ver.empty()) {
            LLVM_GEN_BE_CHECK(detect_macos_deployment_version(triple, &macos_ver));
        }
        if (is_shared) {
            out_args->push_back("-dylib");
        }
        out_args->push_back("-arch");
        out_args->push_back(triple.isAArch64() ? "arm64" : "x86_64");
        out_args->push_back("-platform_version");
        out_args->push_back("macos");
        out_args->push_back(macos_ver);
        out_args->push_back(macos_ver);
        if (!tc.sysroot_path.empty()) {
            out_args->push_back("-syslibroot");
            out_args->push_back(tc.sysroot_path);
        }
        for (const std::string& dir : tc.lib_search_dirs) {
            out_args->push_back("-L" + dir);
        }
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        if (!tc.cxx_stdlib_flag.empty()) out_args->push_back(tc.cxx_stdlib_flag);
        if (!tc.unwind_lib_flag.empty()) out_args->push_back(tc.unwind_lib_flag);
        if (be_state) {
            LLVM_GEN_BE_CHECK(append_extra_linker_flags(be_state->extra_linker_args, true, out_args));
        }
        out_args->push_back("-o");
        out_args->push_back(exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (triple.isWasm()) {
        out_args->push_back("--no-entry");
        out_args->push_back("--export-all");
        for (const std::string& dir : tc.lib_search_dirs) {
            out_args->push_back("-L" + dir);
        }
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        if (be_state) {
            LLVM_GEN_BE_CHECK(append_extra_linker_flags(be_state->extra_linker_args, true, out_args));
        }
        out_args->push_back("-o");
        out_args->push_back(exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (!tc.sysroot_path.empty()) {
        out_args->push_back("--sysroot=" + tc.sysroot_path);
    }
    if (is_shared) {
        out_args->push_back("-shared");
    } else if (is_static) {
        out_args->push_back("-static");
    } else {
        if (pie_mode >= 0) out_args->push_back("-pie");
        if (!tc.dynamic_linker_path.empty()) {
            out_args->push_back("-dynamic-linker");
            out_args->push_back(tc.dynamic_linker_path);
        }
    }
    if (is_rdynamic) {
        out_args->push_back("--export-dynamic");
    }
    if (!is_shared && !tc.crt1_path.empty()) out_args->push_back(tc.crt1_path);
    if (!tc.crti_path.empty()) out_args->push_back(tc.crti_path);
    if (!tc.crtbegin_path.empty()) out_args->push_back(tc.crtbegin_path);
    for (const std::string& dir : tc.lib_search_dirs) {
        out_args->push_back("-L" + dir);
    }
    for (const std::string& obj : obj_files) {
        out_args->push_back(obj);
    }
    if (is_pthread) out_args->push_back("-lpthread");
    if (!tc.cxx_stdlib_flag.empty()) out_args->push_back(tc.cxx_stdlib_flag);
    out_args->push_back("-lm");
    if (!tc.unwind_lib_flag.empty()) out_args->push_back(tc.unwind_lib_flag);
    out_args->push_back("-lc");
    if (!tc.crtend_path.empty()) out_args->push_back(tc.crtend_path);
    if (!tc.crtn_path.empty()) out_args->push_back(tc.crtn_path);
    if (be_state) {
        LLVM_GEN_BE_CHECK(append_extra_linker_flags(be_state->extra_linker_args, true, out_args));
    }
    out_args->push_back("-o");
    out_args->push_back(exe_file_path);
    return llvm_gen_be_error_t::ok;
}

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
    const char* exe_file_path) noexcept {
    if (obj_files.empty() || !exe_file_path || !be_state || !be_state->module) {
        return llvm_gen_be_error_t::invalid_argument;
    }

    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    llvm_toolchain_config_t tc = {};
    LLVM_GEN_BE_CHECK(discover_target_toolchain(triple, nullptr, &tc));
    if (tc.linker_program.empty()) {
        return llvm_gen_be_error_t::code_gen_failure;
    }

    std::vector<std::string> owned_args;
    LLVM_GEN_BE_CHECK(
        build_native_linker_args(tc, triple, obj_files, exe_file_path, &owned_args));

    std::vector<llvm::StringRef> args;
    args.reserve(owned_args.size());
    for (const std::string& s : owned_args) {
        args.push_back(s);
    }
    int ret = llvm::sys::ExecuteAndWait(tc.linker_program, args);
    if (ret != 0) {
        return llvm_gen_be_error_t::code_gen_failure;
    }

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Invokes the system linker to create an executable.
 * @details Detects system toolchains, libraries, and SDKs and executes the driver via `llvm_link_executables`.
 * @param[in] obj_file_path The path to the object file to link.
 * @param[in] exe_file_path The desired path for the final executable.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_link_executable(const char* obj_file_path, const char* exe_file_path) noexcept {
    if (!obj_file_path || !exe_file_path) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    std::vector<std::string> all_inputs = {std::string(obj_file_path)};
    if (be_state) {
        for (const std::string& extra_in : be_state->extra_linker_inputs) {
            all_inputs.push_back(extra_in);
        }
    }
    return llvm_link_executables(all_inputs, exe_file_path);
}

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
    const char* output_exe_path) noexcept {
    if (bitcode_files.empty() || !be_state) {
        return llvm_gen_be_error_t::invalid_argument;
    }

#if LLVM_VERSION_MAJOR >= 22
    be_state->context->setDiagnosticHandlerCallBack(
        [](const llvm::DiagnosticInfo*, void*) noexcept {}, nullptr);
#else
    be_state->context->setDiagnosticHandlerCallBack(
        [](const llvm::DiagnosticInfo&, void*) noexcept {}, nullptr);
#endif

    llvm::Module* raw_combined_mod = nullptr;
    LLVM_GEN_BE_NEW(raw_combined_mod, llvm::Module("lto_combined_module", *be_state->context));
    std::unique_ptr<llvm::Module> combined_mod{raw_combined_mod};
    llvm::Linker ir_linker{*combined_mod};
    llvm::ModuleSummaryIndex combined_index{/*HaveGVs=*/true};

    for (const std::string& bc_path : bitcode_files) {
        auto buf_or_err = llvm::MemoryBuffer::getFile(bc_path);
        if (!buf_or_err) {
            return llvm_gen_be_error_t::io_error;
        }
        auto mod_or_err = llvm::parseBitcodeFile(buf_or_err.get()->getMemBufferRef(), *be_state->context);
        if (!mod_or_err) {
            llvm::consumeError(mod_or_err.takeError());
            return llvm_gen_be_error_t::io_error;
        }
        std::unique_ptr<llvm::Module> sub_mod = std::move(*mod_or_err);
        if (!llvm::Triple(sub_mod->getTargetTriple()).str().empty()) {
            combined_mod->setTargetTriple(sub_mod->getTargetTriple());
        }

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
        llvm::ModuleSummaryIndex& mod_summary = mam.getResult<llvm::ModuleSummaryIndexAnalysis>(*sub_mod);
        for (const auto& entry : mod_summary) {
            combined_index.getOrInsertValueInfo(entry.first);
        }

        if (ir_linker.linkInModule(std::move(sub_mod))) {
            return llvm_gen_be_error_t::code_gen_failure;
        }
    }

    combined_index.setWithGlobalValueDeadStripping();
    be_state->module = std::move(combined_mod);
    a_const_char* saved_ofile = ofile_name;
    ofile_name = output_exe_path;
    llvm_gen_be_error_t lto_err = invoke_embedded_lld_lto();
    ofile_name = saved_ofile;
    return lto_err;
}

/**
 * @brief Performs direct embedded libLLVM LTO optimization and TargetMachine object emission.
 * @details Runs the LTO optimization pipeline on `be_state->module`, emits a native object
 *          artifact via `TargetMachine`, and invokes `llvm_link_executable` when `ofile_name` is set.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t invoke_embedded_lld_lto(void) noexcept {
    if (!be_state || !be_state->module) {
        return llvm_gen_be_error_t::invalid_argument;
    }

    if (!be_state->module->getModuleFlag("FullLTO") && !be_state->module->getModuleFlag("ThinLTO")) {
        be_state->module->addModuleFlag(llvm::Module::Error, "FullLTO", 1);
    }

    llvm_opt_options_t opts = {};
    opts.opt_level = be_state->opt_level;
    opts.use_lto = true;
    llvm_gen_be_error_t opt_err = run_optimization_pipeline(be_state->module.get(), &opts);
    if (opt_err != llvm_gen_be_error_t::ok) {
        return opt_err;
    }

    llvm_gen_be_error_t init_err = initialize_llvm_targets();
    if (init_err != llvm_gen_be_error_t::ok) {
        return init_err;
    }

    std::string triple_str = llvm::Triple(be_state->module->getTargetTriple()).str();
    if (triple_str.empty()) {
        triple_str = "x86_64-unknown-linux-gnu";
    }

    llvm_codegen_opt_level_t cg_opt = llvm_codegen_opt_level_t::Default;
    switch (be_state->opt_level) {
        case llvm_opt_level_t::O0: cg_opt = llvm_codegen_opt_level_t::None; break;
        case llvm_opt_level_t::O1: cg_opt = llvm_codegen_opt_level_t::Less; break;
        case llvm_opt_level_t::O2:
        case llvm_opt_level_t::Os:
        case llvm_opt_level_t::Oz: cg_opt = llvm_codegen_opt_level_t::Default; break;
        case llvm_opt_level_t::O3: cg_opt = llvm_codegen_opt_level_t::Aggressive; break;
        default: cg_opt = llvm_codegen_opt_level_t::Default; break;
    }

    llvm::TargetMachine* tm = nullptr;
    llvm_gen_be_error_t tm_err = create_target_machine(
        triple_str.c_str(), nullptr, nullptr, cg_opt, std::nullopt, std::nullopt, &tm);
    if (tm_err != llvm_gen_be_error_t::ok) {
        return tm_err;
    }

    const char* obj_out_path = gen_obj_file_name ? gen_obj_file_name : "edg_lto_tmp.o";
    llvm_gen_be_error_t emit_err = emit_machine_code_to_file(
        be_state->module.get(), tm, codegen_file_type_t::object_file, obj_out_path);
    if (emit_err != llvm_gen_be_error_t::ok) {
        return emit_err;
    }

    extern a_const_char* ofile_name;
    if (ofile_name) {
        llvm_gen_be_error_t link_err = llvm_link_executable(obj_out_path, ofile_name);
        if (!gen_obj_file_name) {
            llvm::sys::fs::remove(obj_out_path);
        }
        return link_err;
    }

    return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE
#endif
