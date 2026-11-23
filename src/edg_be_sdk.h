/**
 * @file edg_be_sdk.h
 * @brief Host and target SDK, sysroot, compiler resource-dir, and C/C++ STL header discovery.
 * @details Provides non-throwing, deterministic probing of macOS SDKs, Linux glibc/musl
 * multiarch and libstdc++/libc++ trees, and Windows MSVC/MinGW UCRT include hierarchies.
 * @ingroup edg_be_sdk
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef EDG_BE_SDK_H
#define EDG_BE_SDK_H 1

#include "edg_be_common.h"

/**
 * @defgroup edg_be_sdk Host Toolchain & System Header Discovery
 * @ingroup edg_be_common
 * @brief Deterministic discovery of host/target sysroots, builtin headers, and C++ STL paths.
 * @details Probes macOS, Linux (glibc/musl), and Windows (MSVC/MinGW) SDK layouts with zero exceptions.
 * @{
 */

/**
 * @def EDG_BE_SDK_MAX_DIRS
 * @brief Maximum number of discovered system include directories stored in `edg_be_sdk_result_t`.
 * @details Bounded fixed storage eliminates heap allocations and OOM hazards during early setup.
 */
#define EDG_BE_SDK_MAX_DIRS 24U

/**
 * @def EDG_BE_SDK_MAX_PATH_LEN
 * @brief Maximum byte capacity (including null terminator) of each discovered path buffer.
 * @details Sufficient for macOS Xcode, Linux multiarch, and Windows Kits include paths.
 */
#define EDG_BE_SDK_MAX_PATH_LEN 512U

BEGIN_EDG_NAMESPACE

/**
 * @enum edg_host_os_kind_t
 * @brief Operating system family targeted by SDK and system-header discovery.
 * @details Parameterized at runtime so macOS, Linux, and Windows discovery logic can all be
 * exercised and branch-tested on any host platform.
 */
enum class edg_host_os_kind_t : uint8_t {
  darwin = 0,    ///< Apple macOS / Darwin SDK layout.
  linux_os = 1,  ///< Linux FHS layout (glibc multiarch or musl).
  windows_os = 2 ///< Windows MSVC + Windows Kits or MSYS2/MinGW UCRT layout.
};

/**
 * @enum edg_toolchain_flavor_t
 * @brief Front-end emulation mode matching the discovered system headers.
 * @details Determines whether EDG configures `--clang`, `--gcc`/`--g++`, or `--microsoft` mode.
 */
enum class edg_toolchain_flavor_t : uint8_t {
  clang_gnu = 0, ///< Clang-compatible GNU/Apple/LLVM header emulation.
  gcc_gnu = 1,   ///< GCC-compatible libstdc++/glibc/MinGW header emulation.
  msvc_cl = 2    ///< Microsoft Visual C++ + Windows SDK header emulation.
};

/**
 * @typedef edg_be_path_exists_fn_t
 * @brief Callback predicate testing whether a filesystem directory or file path exists.
 * @details Defaults to `edg_be_host_path_exists`; overridden in unit tests to simulate SDK trees.
 */
typedef edg_be_driver_error_t (*edg_be_path_exists_fn_t)(
    const char* path,
    void* user_data,
    bool* out_exists) noexcept;

/**
 * @struct edg_be_sdk_probe_env_t
 * @brief Input environment parameters controlling SDK and STL header discovery.
 * @details Initialized via `edg_be_sdk_probe_env_init` from the active host OS and environment
 * variables, with optional overrides for cross-compilation (`--sysroot`, `--target`) or tests.
 */
struct edg_be_sdk_probe_env_t {
  edg_host_os_kind_t host_os;             ///< Target OS family to probe.
  const char* arch_name;                  ///< Architecture identifier (e.g., "x86_64", "aarch64").
  const char* explicit_sysroot;           ///< Explicit --sysroot / -isysroot path, or nullptr.
  const char* env_sdkroot;                ///< Value of SDKROOT environment variable, or nullptr.
  const char* env_vc_tools_dir;           ///< Value of VCToolsInstallDir on Windows, or nullptr.
  const char* env_win_sdk_dir;            ///< Value of WindowsSdkDir on Windows, or nullptr.
  const char* env_win_sdk_version;        ///< Value of WindowsSdkVersion (e.g., "10.0.22621.0").
  const char* env_mingw_prefix;           ///< Value of MINGW_PREFIX (e.g., "C:/msys64/ucrt64").
  bool nostdinc;                          ///< Skip all system C and C++ include discovery when true.
  bool nostdincxx;                        ///< Skip C++ STL include discovery when true.
  bool nobuiltininc;                      ///< Skip compiler builtin resource include dir when true.
  size_t max_dirs_limit;                  ///< Effective directory list cap (0 uses EDG_BE_SDK_MAX_DIRS).
  size_t max_path_limit;                  ///< Effective path byte cap (0 uses EDG_BE_SDK_MAX_PATH_LEN).
  edg_be_path_exists_fn_t path_exists_fn; ///< Filesystem existence probe callback.
  void* user_data;                        ///< Opaque context pointer forwarded to `path_exists_fn`.
};

/**
 * @struct edg_be_sdk_result_t
 * @brief Discovered system include paths, toolchain flavor, and target triple metadata.
 * @details Owns all path strings inline inside fixed buffers for lifetime safety and zero leaks.
 */
struct edg_be_sdk_result_t {
  edg_toolchain_flavor_t flavor;                                          ///< Matched compiler emulation flavor.
  uint32_t compiler_version_code;                                         ///< Emulated Clang/GCC/MSVC version number.
  char detected_triple[128];                                              ///< Normalized target triple string.
  char detected_sysroot[EDG_BE_SDK_MAX_PATH_LEN];                         ///< Resolved SDK or sysroot directory, if any.
  char detected_resource_dir[EDG_BE_SDK_MAX_PATH_LEN];                    ///< Resolved builtin resource include dir, if any.
  char cxx_include_storage[EDG_BE_SDK_MAX_DIRS][EDG_BE_SDK_MAX_PATH_LEN]; ///< Inline storage for C++ STL include dirs.
  const char* cxx_include_dirs[EDG_BE_SDK_MAX_DIRS];                      ///< Pointers into `cxx_include_storage`.
  size_t num_cxx_include_dirs;                                            ///< Number of discovered C++ STL include dirs.
  char c_include_storage[EDG_BE_SDK_MAX_DIRS][EDG_BE_SDK_MAX_PATH_LEN];   ///< Inline storage for C/system include dirs.
  const char* c_include_dirs[EDG_BE_SDK_MAX_DIRS];                        ///< Pointers into `c_include_storage`.
  size_t num_c_include_dirs;                                              ///< Number of discovered C/system include dirs.
};

/**
 * @brief Default host filesystem existence predicate backed by POSIX/CRT `stat`.
 * @details Returns `false` in `*out_exists` if `path` is null or empty, or if `stat` fails.
 * @param[in] path Null-terminated filesystem path to check.
 * @param[in] user_data Unused opaque callback pointer (may be null).
 * @param[out] out_exists Pointer to receive `true` if `path` exists on disk.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_exists` is null.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_host_path_exists(
    const char* path,
    void* user_data,
    bool* out_exists) EDG_BE_NOEXCEPT;

/**
 * @brief Initializes an `edg_be_sdk_probe_env_t` structure for the running host environment.
 * @details Reads `SDKROOT`, `VCToolsInstallDir`, `WindowsSdkDir`, `WindowsSdkVersion`, and
 * `MINGW_PREFIX` from `std::getenv` and wires the default host `stat` existence callback.
 * @param[out] out_env Pointer to the probe environment structure to initialize.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_env` is null.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_sdk_probe_env_init(
    edg_be_sdk_probe_env_t* out_env) EDG_BE_NOEXCEPT;

/**
 * @brief Discovers system C, compiler resource, and C++ STL include directories for `env`.
 * @details Supports macOS CommandLineTools/Xcode SDKs + Homebrew/Apple Clang resource dirs,
 * Linux glibc/musl multiarch + libstdc++/libc++ versions, and Windows MSVC + Windows Kits /
 * MinGW UCRT layouts without throwing exceptions or allocating heap memory.
 * @param[in] env Pointer to initialized probe parameters.
 * @param[out] out_result Pointer to receive discovered paths and toolchain configuration.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null pointers,
 * `out_of_memory` if a path exceeds capacity, or `sdk_discovery_failed` when no SDK exists.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_sdk_discover(
    const edg_be_sdk_probe_env_t* env,
    edg_be_sdk_result_t* out_result) EDG_BE_NOEXCEPT;

END_EDG_NAMESPACE

/** @} */

#endif /* EDG_BE_SDK_H */
