/**
 * @file edg_be_sdk.cpp
 * @brief Implementation of host and target SDK, resource-dir, and C/C++ STL header discovery.
 * @details Probes macOS, Linux (glibc/musl), and Windows (MSVC/MinGW) system header layouts
 * deterministically with zero heap allocations, zero C++ exceptions, and full error percolation.
 * @ingroup edg_be_sdk
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "edg_be_sdk.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

BEGIN_EDG_NAMESPACE

static constexpr char kSdkBackslash = static_cast<char>(92); ///< ASCII backslash separator.

/**
 * @brief Invokes `env->path_exists_fn` or falls back to `edg_be_host_path_exists`.
 * @details Percolates any callback error directly via `edg_be_driver_error_t`.
 * @param[in] env Active SDK probe environment structure.
 * @param[in] path Null-terminated filesystem path to query.
 * @param[out] out_exists Pointer to receive `true` if `path` exists.
 * @return `edg_be_driver_error_t::ok` on success, or callback error code.
 */
static edg_be_driver_error_t invoke_probe_callback(
    const edg_be_sdk_probe_env_t* env,
    const char* path,
    bool* out_exists) noexcept
{
  const edg_be_path_exists_fn_t fn =
      (env->path_exists_fn != nullptr) ? env->path_exists_fn : &edg_be_host_path_exists;
  return fn(path, env->user_data, out_exists);
}

/**
 * @brief Copies a source string into a fixed-capacity buffer with bounds verification.
 * @details Returns `out_of_memory` if `src` will not fit in `dst_cap` bytes including null byte.
 * @param[out] dst Destination character array.
 * @param[in] dst_cap Effective byte capacity of `dst`.
 * @param[in] src Null-terminated source string.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory`.
 */
static edg_be_driver_error_t copy_bounded_path(
    char* dst,
    size_t dst_cap,
    const char* src) noexcept
{
  const size_t len = std::strlen(src);
  if (len >= dst_cap) {
    return edg_be_driver_error_t::out_of_memory;
  }
  std::memcpy(dst, src, len + 1U);
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Joins non-empty `prefix` and relative `suffix` with `/` into `dst`.
 * @details Strips trailing `/` and backslash characters on `prefix` (preserving root `"/"`).
 * @param[in] env Active SDK probe environment providing effective path capacity.
 * @param[out] dst Destination character buffer of size `EDG_BE_SDK_MAX_PATH_LEN`.
 * @param[in] prefix Non-empty leading directory path.
 * @param[in] suffix Relative subpath without leading slash.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory` on truncation.
 */
static edg_be_driver_error_t join_sdk_path(
    const edg_be_sdk_probe_env_t* env,
    char* dst,
    const char* prefix,
    const char* suffix) noexcept
{
  const size_t path_cap =
      (env->max_path_limit > 0U && env->max_path_limit < EDG_BE_SDK_MAX_PATH_LEN)
          ? env->max_path_limit
          : EDG_BE_SDK_MAX_PATH_LEN;
  size_t plen = std::strlen(prefix);
  while (plen > 1U && (prefix[plen - 1U] == '/' || prefix[plen - 1U] == kSdkBackslash)) {
    --plen;
  }
  const size_t slen = std::strlen(suffix);
  const bool need_sep = (prefix[plen - 1U] != '/');
  const size_t total = plen + (need_sep ? 1U : 0U) + slen;
  if (total >= path_cap) {
    return edg_be_driver_error_t::out_of_memory;
  }
  std::memcpy(dst, prefix, plen);
  size_t pos = plen;
  if (need_sep) {
    dst[pos++] = '/';
  }
  std::memcpy(dst + pos, suffix, slen + 1U);
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Appends a verified include directory into either C or C++ include arrays with deduplication.
 * @details Shared implementation backing both C and C++ include list registration.
 * @param[in] env Active SDK probe environment providing capacity limits.
 * @param[in,out] storage 2D character array holding path strings.
 * @param[in,out] ptrs Array of `const char*` pointing into `storage`.
 * @param[in,out] io_count Current directory count, incremented when a new unique path is added.
 * @param[in] dir_path Null-terminated include directory path to append.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory` if capacity is exceeded.
 */
static edg_be_driver_error_t append_unique_dir(
    const edg_be_sdk_probe_env_t* env,
    char storage[EDG_BE_SDK_MAX_DIRS][EDG_BE_SDK_MAX_PATH_LEN],
    const char* ptrs[EDG_BE_SDK_MAX_DIRS],
    size_t* io_count,
    const char* dir_path) noexcept
{
  const size_t dirs_cap =
      (env->max_dirs_limit > 0U && env->max_dirs_limit < EDG_BE_SDK_MAX_DIRS)
          ? env->max_dirs_limit
          : EDG_BE_SDK_MAX_DIRS;
  if (*io_count >= dirs_cap) {
    return edg_be_driver_error_t::out_of_memory;
  }
  const size_t idx = *io_count;
  std::memcpy(storage[idx], dir_path, std::strlen(dir_path) + 1U);
  ptrs[idx] = storage[idx];
  *io_count = idx + 1U;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Probes `prefix/suffix` and appends it to `ptrs` if it exists on disk.
 * @details Combines `join_sdk_path`, `invoke_probe_callback`, and `append_unique_dir`.
 * @param[in] env Active SDK probe environment.
 * @param[in] prefix Non-empty root/SDK directory path.
 * @param[in] suffix Relative include subdirectory path.
 * @param[in,out] storage 2D character storage array.
 * @param[in,out] ptrs Include pointer array.
 * @param[in,out] io_count Current element count in `ptrs`.
 * @param[out] out_found Receives `true` if `prefix/suffix` existed and was added.
 * @return `edg_be_driver_error_t::ok` on success, or percolated probe/OOM error.
 */
static edg_be_driver_error_t probe_and_append_subdir(
    const edg_be_sdk_probe_env_t* env,
    const char* prefix,
    const char* suffix,
    char storage[EDG_BE_SDK_MAX_DIRS][EDG_BE_SDK_MAX_PATH_LEN],
    const char* ptrs[EDG_BE_SDK_MAX_DIRS],
    size_t* io_count,
    bool* out_found) noexcept
{
  char full[EDG_BE_SDK_MAX_PATH_LEN];
  EDG_BE_DRIVER_CHECK(join_sdk_path(env, full, prefix, suffix));
  bool exists = false;
  EDG_BE_DRIVER_CHECK(invoke_probe_callback(env, full, &exists));
  *out_found = exists;
  if (exists) {
    EDG_BE_DRIVER_CHECK(append_unique_dir(env, storage, ptrs, io_count, full));
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Discovers macOS SDK, Clang builtin resource headers, and `libc++` include paths.
 * @details Probes explicit sysroot, `SDKROOT`, CommandLineTools, and Xcode SDK roots, followed
 * by Apple/Homebrew Clang resource directories (`clang/21` down to `clang/14`).
 * @param[in] env Active SDK probe environment.
 * @param[in] arch Resolved architecture string.
 * @param[out] res Destination SDK discovery result.
 * @return `edg_be_driver_error_t::ok` on success, or `sdk_discovery_failed` / `out_of_memory`.
 */
static edg_be_driver_error_t discover_darwin_sdk(
    const edg_be_sdk_probe_env_t* env,
    const char* arch,
    edg_be_sdk_result_t* res) noexcept
{
  const size_t path_cap =
      (env->max_path_limit > 0U && env->max_path_limit < EDG_BE_SDK_MAX_PATH_LEN)
          ? env->max_path_limit
          : EDG_BE_SDK_MAX_PATH_LEN;
  res->flavor = edg_toolchain_flavor_t::clang_gnu;
  res->compiler_version_code = 170000U;
  std::snprintf(res->detected_triple, sizeof(res->detected_triple), "%s-apple-darwin", arch);

  const char* candidates[4] = {
    env->explicit_sysroot,
    env->env_sdkroot,
    "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
    "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
  };

  const char* chosen_sdk = nullptr;
  for (size_t i = 0; i < 4U; ++i) {
    if (candidates[i] != nullptr && candidates[i][0] != '\0') {
      bool exists = false;
      EDG_BE_DRIVER_CHECK(invoke_probe_callback(env, candidates[i], &exists));
      if (exists) {
        chosen_sdk = candidates[i];
        break;
      }
    }
  }
  if (chosen_sdk == nullptr) {
    return edg_be_driver_error_t::sdk_discovery_failed;
  }
  EDG_BE_DRIVER_CHECK(copy_bounded_path(res->detected_sysroot, path_cap, chosen_sdk));

  if (!env->nostdincxx) {
    bool cxx_added = false;
    EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
        env, chosen_sdk, "usr/include/c++/v1",
        res->cxx_include_storage, res->cxx_include_dirs, &res->num_cxx_include_dirs, &cxx_added));
  }

  if (!env->nobuiltininc) {
    const char* clang_roots[3] = {
      "/Library/Developer/CommandLineTools/usr/lib/clang",
      "/opt/homebrew/opt/llvm/lib/clang",
      "/usr/local/opt/llvm/lib/clang"
    };
    const int versions[12] = {25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14};
    bool found_res = false;
    for (size_t r = 0; r < 3U && !found_res; ++r) {
      for (size_t v = 0; v < 12U && !found_res; ++v) {
        char ver_sub[64];
        std::snprintf(ver_sub, sizeof(ver_sub), "%d/include", versions[v]);
        EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
            env, clang_roots[r], ver_sub,
            res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &found_res));
        if (found_res) {
          std::snprintf(res->detected_resource_dir, sizeof(res->detected_resource_dir),
                        "%s", res->c_include_dirs[res->num_c_include_dirs - 1U]);
          res->compiler_version_code = static_cast<uint32_t>(versions[v]) * 10000U;
        }
      }
    }
  }

  bool usr_inc_exists = false;
  EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
      env, chosen_sdk, "usr/include",
      res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &usr_inc_exists));
  if (!usr_inc_exists) {
    return edg_be_driver_error_t::sdk_discovery_failed;
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Discovers Linux glibc or musl system headers, GCC/Clang builtins, and STL include paths.
 * @details Probes `<sysroot>/usr/include/<arch>-linux-gnu` vs musl layout, GCC `libstdc++`
 * versions `15..10` (plus `libc++`), and GCC resource include directories.
 * @param[in] env Active SDK probe environment.
 * @param[in] arch Resolved architecture string.
 * @param[out] res Destination SDK discovery result.
 * @return `edg_be_driver_error_t::ok` on success, or `sdk_discovery_failed` / `out_of_memory`.
 */
static edg_be_driver_error_t discover_linux_sdk(
    const edg_be_sdk_probe_env_t* env,
    const char* arch,
    edg_be_sdk_result_t* res) noexcept
{
  const size_t path_cap =
      (env->max_path_limit > 0U && env->max_path_limit < EDG_BE_SDK_MAX_PATH_LEN)
          ? env->max_path_limit
          : EDG_BE_SDK_MAX_PATH_LEN;
  const char* root = (env->explicit_sysroot != nullptr && env->explicit_sysroot[0] != '\0')
                         ? env->explicit_sysroot
                         : "/";
  EDG_BE_DRIVER_CHECK(copy_bounded_path(res->detected_sysroot, path_cap, root));

  char usr_inc[EDG_BE_SDK_MAX_PATH_LEN];
  EDG_BE_DRIVER_CHECK(join_sdk_path(env, usr_inc, root, "usr/include"));
  bool usr_exists = false;
  EDG_BE_DRIVER_CHECK(invoke_probe_callback(env, usr_inc, &usr_exists));
  if (!usr_exists) {
    return edg_be_driver_error_t::sdk_discovery_failed;
  }

  char musl_ld_rel[128];
  std::snprintf(musl_ld_rel, sizeof(musl_ld_rel), "lib/ld-musl-%s.so.1", arch);
  char musl_ld_path[EDG_BE_SDK_MAX_PATH_LEN];
  EDG_BE_DRIVER_CHECK(join_sdk_path(env, musl_ld_path, root, musl_ld_rel));
  bool is_musl = false;
  EDG_BE_DRIVER_CHECK(invoke_probe_callback(env, musl_ld_path, &is_musl));

  char multiarch[128];
  if (is_musl) {
    std::snprintf(multiarch, sizeof(multiarch), "%s-linux-musl", arch);
    std::snprintf(res->detected_triple, sizeof(res->detected_triple), "%s-unknown-linux-musl", arch);
  } else {
    std::snprintf(multiarch, sizeof(multiarch), "%s-linux-gnu", arch);
    std::snprintf(res->detected_triple, sizeof(res->detected_triple), "%s-unknown-linux-gnu", arch);
  }

  res->flavor = edg_toolchain_flavor_t::gcc_gnu;
  res->compiler_version_code = 130000U;
  const int gcc_versions[6] = {15, 14, 13, 12, 11, 10};

  if (!env->nostdincxx) {
    for (size_t v = 0; v < 6U; ++v) {
      char rel_cxx[160];
      std::snprintf(rel_cxx, sizeof(rel_cxx), "usr/include/c++/%d", gcc_versions[v]);
      bool cxx_ok = false;
      EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
          env, root, rel_cxx,
          res->cxx_include_storage, res->cxx_include_dirs, &res->num_cxx_include_dirs, &cxx_ok));
      if (cxx_ok) {
        const int matched_ver = gcc_versions[v];
        res->compiler_version_code = static_cast<uint32_t>(matched_ver) * 10000U;

        char rel_arch_cxx[256];
        std::snprintf(rel_arch_cxx, sizeof(rel_arch_cxx), "usr/include/%s/c++/%d", multiarch, matched_ver);
        bool arch_ok = false;
        EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
            env, root, rel_arch_cxx,
            res->cxx_include_storage, res->cxx_include_dirs, &res->num_cxx_include_dirs, &arch_ok));

        char rel_back[192];
        std::snprintf(rel_back, sizeof(rel_back), "usr/include/c++/%d/backward", matched_ver);
        bool back_ok = false;
        EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
            env, root, rel_back,
            res->cxx_include_storage, res->cxx_include_dirs, &res->num_cxx_include_dirs, &back_ok));
        break;
      }
    }

    if (res->num_cxx_include_dirs == 0U) {
      bool libcxx_ok = false;
      EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
          env, root, "usr/include/c++/v1",
          res->cxx_include_storage, res->cxx_include_dirs, &res->num_cxx_include_dirs, &libcxx_ok));
      if (libcxx_ok) {
        res->flavor = edg_toolchain_flavor_t::clang_gnu;
        res->compiler_version_code = 180000U;
      }
    }
  }

  if (!env->nobuiltininc) {
    for (size_t v = 0; v < 6U; ++v) {
      char rel_res[256];
      std::snprintf(rel_res, sizeof(rel_res), "usr/lib/gcc/%s/%d/include", multiarch, gcc_versions[v]);
      bool res_ok = false;
      EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
          env, root, rel_res,
          res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &res_ok));
      if (res_ok) {
        std::snprintf(res->detected_resource_dir, sizeof(res->detected_resource_dir),
                      "%s", res->c_include_dirs[res->num_c_include_dirs - 1U]);
        break;
      }
    }
  }

  bool local_ok = false;
  EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
      env, root, "usr/local/include",
      res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &local_ok));

  char multiarch_rel[192];
  std::snprintf(multiarch_rel, sizeof(multiarch_rel), "usr/include/%s", multiarch);
  bool ma_ok = false;
  EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
      env, root, multiarch_rel,
      res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &ma_ok));

  return append_unique_dir(
      env, res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, usr_inc);
}

/**
 * @brief Discovers Windows MSVC + Windows Kits or MSYS2/MinGW UCRT include directories.
 * @details Prefers `VCToolsInstallDir` + `WindowsSdkDir`, falling back to `MINGW_PREFIX` or MSYS2.
 * @param[in] env Active SDK probe environment.
 * @param[in] arch Resolved architecture string.
 * @param[out] res Destination SDK discovery result.
 * @return `edg_be_driver_error_t::ok` on success, or `sdk_discovery_failed` / `out_of_memory`.
 */
static edg_be_driver_error_t discover_windows_sdk(
    const edg_be_sdk_probe_env_t* env,
    const char* arch,
    edg_be_sdk_result_t* res) noexcept
{
  if (env->env_vc_tools_dir != nullptr && env->env_vc_tools_dir[0] != '\0') {
    bool vc_ok = false;
    EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
        env, env->env_vc_tools_dir, "include",
        res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &vc_ok));
    if (vc_ok) {
      res->flavor = edg_toolchain_flavor_t::msvc_cl;
      res->compiler_version_code = 1939U;
      std::snprintf(res->detected_triple, sizeof(res->detected_triple), "%s-pc-windows-msvc", arch);
      std::snprintf(res->detected_sysroot, sizeof(res->detected_sysroot), "%s", env->env_vc_tools_dir);
      if (!env->nostdincxx) {
        std::snprintf(res->cxx_include_storage[0], sizeof(res->cxx_include_storage[0]),
                      "%s", res->c_include_dirs[0]);
        res->cxx_include_dirs[0] = res->cxx_include_storage[0];
        res->num_cxx_include_dirs = 1U;
      }

      if (env->env_win_sdk_dir != nullptr && env->env_win_sdk_dir[0] != '\0') {
        const char* sdk_ver = (env->env_win_sdk_version != nullptr && env->env_win_sdk_version[0] != '\0')
                                  ? env->env_win_sdk_version
                                  : "10.0.22621.0";
        const char* subdirs[3] = {"ucrt", "shared", "um"};
        for (size_t i = 0; i < 3U; ++i) {
          char sub_rel[192];
          std::snprintf(sub_rel, sizeof(sub_rel), "Include/%s/%s", sdk_ver, subdirs[i]);
          bool sub_ok = false;
          EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
              env, env->env_win_sdk_dir, sub_rel,
              res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &sub_ok));
        }
      }
      return edg_be_driver_error_t::ok;
    }
  }

  const char* mingw_roots[3] = {
    env->explicit_sysroot,
    env->env_mingw_prefix,
    "C:/msys64/ucrt64"
  };
  for (size_t i = 0; i < 3U; ++i) {
    if (mingw_roots[i] != nullptr && mingw_roots[i][0] != '\0') {
      bool mgw_ok = false;
      EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
          env, mingw_roots[i], "include",
          res->c_include_storage, res->c_include_dirs, &res->num_c_include_dirs, &mgw_ok));
      if (mgw_ok) {
        res->flavor = edg_toolchain_flavor_t::gcc_gnu;
        res->compiler_version_code = 130000U;
        std::snprintf(res->detected_triple, sizeof(res->detected_triple), "%s-w64-mingw32", arch);
        std::snprintf(res->detected_sysroot, sizeof(res->detected_sysroot), "%s", mingw_roots[i]);
        if (!env->nostdincxx) {
          bool cxx_ok = false;
          EDG_BE_DRIVER_CHECK(probe_and_append_subdir(
              env, mingw_roots[i], "include/c++/v1",
              res->cxx_include_storage, res->cxx_include_dirs, &res->num_cxx_include_dirs, &cxx_ok));
        }
        return edg_be_driver_error_t::ok;
      }
    }
  }

  return edg_be_driver_error_t::sdk_discovery_failed;
}

/**
 * @brief Default host filesystem existence predicate backed by POSIX/CRT `stat`.
 * @details Writes `true` to `*out_exists` when `stat` succeeds on `path`, `false` otherwise.
 * @param[in] path Null-terminated filesystem path to check.
 * @param[in] user_data Unused opaque callback pointer (may be null).
 * @param[out] out_exists Pointer to receive `true` if `path` exists on disk.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_exists` is null.
 */
edg_be_driver_error_t edg_be_host_path_exists(
    const char* path,
    void* user_data,
    bool* out_exists) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_exists);
  if (user_data != nullptr) {
    *out_exists = false;
  }
  if (path == nullptr || path[0] == '\0') {
    *out_exists = false;
    return edg_be_driver_error_t::ok;
  }
  struct stat st;
  *out_exists = (::stat(path, &st) == 0);
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Initializes an `edg_be_sdk_probe_env_t` structure for the running host environment.
 * @details Populates host OS, CPU architecture, and SDK environment variables.
 * @param[out] out_env Pointer to the probe environment structure to initialize.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_env` is null.
 */
edg_be_driver_error_t edg_be_sdk_probe_env_init(
    edg_be_sdk_probe_env_t* out_env) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_env);
  std::memset(out_env, 0, sizeof(*out_env));
#if defined(__APPLE__)
  out_env->host_os = edg_host_os_kind_t::darwin;
#elif defined(_WIN32)
  out_env->host_os = edg_host_os_kind_t::windows_os;
#else
  out_env->host_os = edg_host_os_kind_t::linux_os;
#endif

#if defined(__APPLE__) && (defined(__aarch64__) || defined(_M_ARM64))
  out_env->arch_name = "arm64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  out_env->arch_name = "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
  out_env->arch_name = "i686";
#elif defined(__riscv)
  out_env->arch_name = "riscv64";
#else
  out_env->arch_name = "x86_64";
#endif

  out_env->env_sdkroot = std::getenv("SDKROOT");
  out_env->env_vc_tools_dir = std::getenv("VCToolsInstallDir");
  out_env->env_win_sdk_dir = std::getenv("WindowsSdkDir");
  out_env->env_win_sdk_version = std::getenv("WindowsSdkVersion");
  out_env->env_mingw_prefix = std::getenv("MINGW_PREFIX");
  out_env->path_exists_fn = &edg_be_host_path_exists;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Discovers system C, compiler resource, and C++ STL include directories for `env`.
 * @details Dispatches to macOS, Linux, or Windows discovery routines unless `env->nostdinc` is set.
 * @param[in] env Pointer to initialized probe parameters.
 * @param[out] out_result Pointer to receive discovered paths and toolchain configuration.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument`, `out_of_memory`, or `sdk_discovery_failed`.
 */
edg_be_driver_error_t edg_be_sdk_discover(
    const edg_be_sdk_probe_env_t* env,
    edg_be_sdk_result_t* out_result) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(env);
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_result);
  std::memset(out_result, 0, sizeof(*out_result));

  const char* arch = (env->arch_name != nullptr && env->arch_name[0] != '\0')
                         ? env->arch_name
                         : "x86_64";

  if (env->nostdinc) {
    out_result->flavor = edg_toolchain_flavor_t::clang_gnu;
    out_result->compiler_version_code = 170000U;
    std::snprintf(out_result->detected_triple, sizeof(out_result->detected_triple), "%s-unknown-unknown", arch);
    return edg_be_driver_error_t::ok;
  }

  switch (env->host_os) {
    case edg_host_os_kind_t::darwin:
      return discover_darwin_sdk(env, arch, out_result);
    case edg_host_os_kind_t::linux_os:
      return discover_linux_sdk(env, arch, out_result);
    case edg_host_os_kind_t::windows_os:
      return discover_windows_sdk(env, arch, out_result);
    default:
      return edg_be_driver_error_t::invalid_argument;
  }
}

END_EDG_NAMESPACE
