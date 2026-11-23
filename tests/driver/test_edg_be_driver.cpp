/**
 * @file test_edg_be_driver.cpp
 * @brief Comprehensive unit and branch-coverage suite for `edg_be_common`, `edg_be_sdk`, and `edg_be_driver`.
 * @details Exercises every function, error code, OS SDK layout (macOS, Linux glibc/musl,
 * Windows MSVC/MinGW), CLI flag family, `@rspfile` edge case, query mode, CWD object rule,
 * and Make dependency file escaping branch with zero C++ exceptions and `noexcept` everywhere.
 * @ingroup edg_be_driver
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "edg_be_driver.h"
#include "edg_be_sdk.h"

#include <cstdio>
#include <cstring>

USING_NAMESPACE_EDG

static constexpr char kLf = static_cast<char>(10); ///< ASCII LF (0x0A).
static constexpr char kCr = static_cast<char>(13); ///< ASCII CR (0x0D).
static constexpr char kTab = static_cast<char>(9); ///< ASCII TAB (0x09).
static constexpr char kBs = static_cast<char>(92); ///< ASCII backslash (0x5C).
static constexpr char kQt = static_cast<char>(34); ///< ASCII double-quote (0x22).

/**
 * @def TEST_ASSERT
 * @brief Returns `edg_be_driver_error_t::io_error` with diagnostic line info if `cond` is false.
 * @details Avoids `abort()` / `assert()` so tests obey the zero-abort policy.
 * @param cond Boolean expression expected to evaluate to `true`.
 */
#define TEST_ASSERT(cond) do { if (!(cond)) { std::fprintf(stderr, "Assertion failed at line %d%c", __LINE__, kLf); return edg_be_driver_error_t::io_error; } } while (false)

/**
 * @struct mock_fs_t
 * @brief In-memory virtual directory table used to simulate macOS, Linux, and Windows SDK trees.
 * @details Allows deterministic cross-OS testing on any host machine.
 */
struct mock_fs_t {
  const char* existing_paths[32]; ///< Null-terminated paths simulated as existing on disk.
  size_t count = 0U;              ///< Number of valid entries in `existing_paths`.
  bool force_error = false;       ///< When true, simulates an immediate I/O failure.
  size_t fail_on_call = 0U;       ///< 1-based probe index on which to inject `io_error` (`0` = never).
  size_t call_count = 0U;         ///< Number of callback invocations observed so far.
};

/**
 * @brief Virtual filesystem existence callback matching `edg_be_path_exists_fn_t`.
 * @details Searches `mock_fs_t::existing_paths` or returns `io_error` when `force_error` or `fail_on_call` triggers.
 * @param[in] path Filesystem path queried by `edg_be_sdk_discover`.
 * @param[in] user_data Pointer to `mock_fs_t`.
 * @param[out] out_exists Receives `true` if `path` is listed in `existing_paths`.
 * @return `edg_be_driver_error_t::ok` or `io_error` when fault injection triggers.
 */
static edg_be_driver_error_t mock_path_exists_cb(
    const char* path,
    void* user_data,
    bool* out_exists) noexcept
{
  mock_fs_t* fs = static_cast<mock_fs_t*>(user_data);
  fs->call_count += 1U;
  if (fs->force_error || (fs->fail_on_call > 0U && fs->call_count == fs->fail_on_call)) {
    return edg_be_driver_error_t::io_error;
  }
  *out_exists = false;
  for (size_t i = 0; i < fs->count; ++i) {
    if (std::strcmp(fs->existing_paths[i], path) == 0) {
      *out_exists = true;
      return edg_be_driver_error_t::ok;
    }
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Verifies `edg_be_driver_error_to_string` and initialization helpers across all branches.
 * @details Checks all 17 defined enumerators, unknown enumerator fallback, and null-pointer guards.
 * @return `edg_be_driver_error_t::ok` on success.
 */
static edg_be_driver_error_t test_error_strings_and_inits(void) noexcept
{
  TEST_ASSERT(edg_be_driver_error_to_string(edg_be_driver_error_t::ok, nullptr) ==
              edg_be_driver_error_t::invalid_argument);
  for (int32_t code = 0; code <= 17; ++code) {
    const char* msg = nullptr;
    EDG_BE_DRIVER_CHECK(edg_be_driver_error_to_string(
        static_cast<edg_be_driver_error_t>(code), &msg));
    TEST_ASSERT(msg != nullptr && msg[0] != '\0');
  }
  TEST_ASSERT(edg_be_invocation_options_init(nullptr) == edg_be_driver_error_t::invalid_argument);
  edg_be_invocation_options_t valid_opts;
  EDG_BE_DRIVER_CHECK(edg_be_invocation_options_init(&valid_opts));
  TEST_ASSERT(valid_opts.exceptions_enabled && valid_opts.rtti_enabled);
  TEST_ASSERT(edg_be_translated_command_init(nullptr) == edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_sdk_probe_env_init(nullptr) == edg_be_driver_error_t::invalid_argument);
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Verifies `edg_be_host_path_exists` and `edg_be_sdk_discover` across macOS, Linux, and Windows.
 * @details Covers glibc, musl, libc++, MSVC, MinGW, nostdinc, path/array capacity limits, and failures.
 * @return `edg_be_driver_error_t::ok` on success.
 */
static edg_be_driver_error_t test_sdk_discovery_all_platforms(void) noexcept
{
  bool exists = false;
  TEST_ASSERT(edg_be_host_path_exists(".", nullptr, nullptr) ==
              edg_be_driver_error_t::invalid_argument);
  int dummy_ctx = 1;
  EDG_BE_DRIVER_CHECK(edg_be_host_path_exists(nullptr, &dummy_ctx, &exists));
  TEST_ASSERT(!exists);
  EDG_BE_DRIVER_CHECK(edg_be_host_path_exists("", nullptr, &exists));
  TEST_ASSERT(!exists);
  EDG_BE_DRIVER_CHECK(edg_be_host_path_exists(".", nullptr, &exists));
  TEST_ASSERT(exists);
  EDG_BE_DRIVER_CHECK(edg_be_host_path_exists("/definitely_nonexistent_edg_path_xyz", nullptr, &exists));
  TEST_ASSERT(!exists);

  edg_be_sdk_probe_env_t host_env;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_probe_env_init(&host_env));
  edg_be_sdk_result_t res;
  TEST_ASSERT(edg_be_sdk_discover(nullptr, &res) == edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_sdk_discover(&host_env, nullptr) == edg_be_driver_error_t::invalid_argument);

  /* Default host callback path via nullptr path_exists_fn on nostdinc and real stat */
  host_env.nostdinc = true;
  host_env.arch_name = nullptr;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&host_env, &res));
  TEST_ASSERT(std::strcmp(res.detected_triple, "x86_64-unknown-unknown") == 0);

  host_env.nostdinc = false;
  host_env.host_os = edg_host_os_kind_t::darwin;
  host_env.explicit_sysroot = "/definitely_nonexistent_edg_sdk_xyz";
  host_env.env_sdkroot = "";
  /* Also exercise invalid enum value branch */
  edg_be_sdk_probe_env_t bad_os_env = host_env;
  bad_os_env.host_os = static_cast<edg_host_os_kind_t>(99);
  TEST_ASSERT(edg_be_sdk_discover(&bad_os_env, &res) == edg_be_driver_error_t::invalid_argument);

  /* Invoke real stat callback path via nullptr path_exists_fn with missing Linux sysroot */
  edg_be_sdk_probe_env_t real_cb_env = host_env;
  real_cb_env.host_os = edg_host_os_kind_t::linux_os;
  real_cb_env.explicit_sysroot = "/definitely_nonexistent_edg_sysroot_xyz";
  real_cb_env.path_exists_fn = nullptr;
  TEST_ASSERT(edg_be_sdk_discover(&real_cb_env, &res) ==
              edg_be_driver_error_t::sdk_discovery_failed);

  /* Callback error propagation */
  mock_fs_t err_fs = {{}, 0U, true};
  edg_be_sdk_probe_env_t env = {};
  env.host_os = edg_host_os_kind_t::darwin;
  env.arch_name = "arm64";
  env.explicit_sysroot = "/MacSDK";
  env.path_exists_fn = &mock_path_exists_cb;
  env.user_data = &err_fs;
  TEST_ASSERT(edg_be_sdk_discover(&env, &res) == edg_be_driver_error_t::io_error);

  /* 1. macOS: full discovery, missing SDK, missing usr/include, nostdincxx/nobuiltininc */
  mock_fs_t empty_fs = {{}, 0U, false};
  env.user_data = &empty_fs;
  TEST_ASSERT(edg_be_sdk_discover(&env, &res) == edg_be_driver_error_t::sdk_discovery_failed);

  env.explicit_sysroot = "/MacSDK/";
  mock_fs_t darwin_no_usr2 = {{"/MacSDK/"}, 1U, false};
  env.user_data = &darwin_no_usr2;
  TEST_ASSERT(edg_be_sdk_discover(&env, &res) == edg_be_driver_error_t::sdk_discovery_failed);

  mock_fs_t darwin_full = {{
    "/MacSDK",
    "/MacSDK/usr/include/c++/v1",
    "/opt/homebrew/opt/llvm/lib/clang/19/include",
    "/MacSDK/usr/include"
  }, 4U, false};
  env.explicit_sysroot = nullptr;
  env.env_sdkroot = "/MacSDK";
  env.user_data = &darwin_full;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&env, &res));
  TEST_ASSERT(res.flavor == edg_toolchain_flavor_t::clang_gnu);
  TEST_ASSERT(res.compiler_version_code == 190000U);
  TEST_ASSERT(res.num_cxx_include_dirs == 1U);
  TEST_ASSERT(res.num_c_include_dirs == 2U);

  env.nostdincxx = true;
  env.nobuiltininc = true;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&env, &res));
  TEST_ASSERT(res.num_cxx_include_dirs == 0U && res.num_c_include_dirs == 1U);

  /* Path overflow > EDG_BE_SDK_MAX_PATH_LEN */
  char huge_path[600];
  std::memset(huge_path, 'a', sizeof(huge_path) - 1U);
  huge_path[0] = '/';
  huge_path[sizeof(huge_path) - 1U] = '\0';
  mock_fs_t huge_fs = {{huge_path}, 1U, false};
  env.explicit_sysroot = huge_path;
  env.user_data = &huge_fs;
  TEST_ASSERT(edg_be_sdk_discover(&env, &res) == edg_be_driver_error_t::out_of_memory);

  /* 2. Linux glibc (libstdc++ 14 + resource dir + local + multiarch) and musl (libc++) */
  mock_fs_t linux_glibc = {{
    "/usr/include",
    "/usr/include/c++/14",
    "/usr/include/x86_64-linux-gnu/c++/14",
    "/usr/include/c++/14/backward",
    "/usr/lib/gcc/x86_64-linux-gnu/14/include",
    "/usr/local/include",
    "/usr/include/x86_64-linux-gnu"
  }, 7U, false};
  edg_be_sdk_probe_env_t lenv = {};
  lenv.host_os = edg_host_os_kind_t::linux_os;
  lenv.arch_name = "x86_64";
  lenv.path_exists_fn = &mock_path_exists_cb;
  lenv.user_data = &linux_glibc;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&lenv, &res));
  TEST_ASSERT(res.flavor == edg_toolchain_flavor_t::gcc_gnu);
  TEST_ASSERT(res.compiler_version_code == 140000U);
  TEST_ASSERT(res.num_cxx_include_dirs == 3U);
  TEST_ASSERT(res.num_c_include_dirs == 4U);

  mock_fs_t linux_musl = {{
    "/alpine/usr/include",
    "/alpine/lib/ld-musl-aarch64.so.1",
    "/alpine/usr/include/c++/v1"
  }, 3U, false};
  lenv.arch_name = "aarch64";
  lenv.explicit_sysroot = "/alpine/";
  lenv.user_data = &linux_musl;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&lenv, &res));
  TEST_ASSERT(res.flavor == edg_toolchain_flavor_t::clang_gnu);
  TEST_ASSERT(std::strcmp(res.detected_triple, "aarch64-unknown-linux-musl") == 0);

  lenv.nostdincxx = true;
  lenv.nobuiltininc = true;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&lenv, &res));
  TEST_ASSERT(res.num_cxx_include_dirs == 0U);

  /* 3. Windows MSVC + Windows Kits, MSVC without Kits, MinGW UCRT, and missing */
  mock_fs_t win_msvc = {{
    "C:/VC/include",
    "C:/WinKit/Include/10.0.22621.0/ucrt",
    "C:/WinKit/Include/10.0.22621.0/shared",
    "C:/WinKit/Include/10.0.22621.0/um"
  }, 4U, false};
  char vc_trail[32];
  std::snprintf(vc_trail, sizeof(vc_trail), "C:/VC%c", kBs);
  edg_be_sdk_probe_env_t wenv = {};
  wenv.host_os = edg_host_os_kind_t::windows_os;
  wenv.arch_name = "x86_64";
  wenv.env_vc_tools_dir = vc_trail;
  wenv.env_win_sdk_dir = "C:/WinKit";
  wenv.path_exists_fn = &mock_path_exists_cb;
  wenv.user_data = &win_msvc;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&wenv, &res));
  TEST_ASSERT(res.flavor == edg_toolchain_flavor_t::msvc_cl);
  TEST_ASSERT(res.num_c_include_dirs == 4U);

  wenv.env_win_sdk_version = "10.0.22621.0";
  wenv.nostdincxx = true;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&wenv, &res));
  wenv.env_win_sdk_dir = nullptr;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&wenv, &res));

  mock_fs_t win_mingw = {{
    "C:/msys64/ucrt64/include",
    "C:/msys64/ucrt64/include/c++/v1"
  }, 2U, false};
  wenv.env_vc_tools_dir = "C:/MissingVC";
  wenv.nostdincxx = false;
  wenv.env_mingw_prefix = "C:/msys64/ucrt64";
  wenv.user_data = &win_mingw;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&wenv, &res));
  TEST_ASSERT(res.flavor == edg_toolchain_flavor_t::gcc_gnu);
  TEST_ASSERT(res.num_cxx_include_dirs == 1U);

  wenv.nostdincxx = true;
  EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&wenv, &res));

  wenv.user_data = &empty_fs;
  TEST_ASSERT(edg_be_sdk_discover(&wenv, &res) == edg_be_driver_error_t::sdk_discovery_failed);

  /* Step-exhaustion fault injection across all 3 OS SDK layouts + deduplication */
  mock_fs_t dedup_mac = {{
    "/MacSDK",
    "/MacSDK/usr/include",
    "/Library/Developer/CommandLineTools/usr/lib/clang/21/include"
  }, 3U, false, 0U, 0U};
  for (size_t fail_at = 1U; fail_at <= 8U; ++fail_at) {
    dedup_mac.fail_on_call = fail_at;
    dedup_mac.call_count = 0U;
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::darwin;
    fenv.arch_name = "";
    fenv.explicit_sysroot = "";
    fenv.env_sdkroot = "/MacSDK";
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &dedup_mac;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::io_error || rc == edg_be_driver_error_t::ok);
  }
  dedup_mac.fail_on_call = 0U;
  /* Trigger duplicate include deduplication by presetting c_include_dirs via 2-step limits */
  for (size_t lim = 1U; lim <= 60U; ++lim) {
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::darwin;
    fenv.arch_name = "arm64";
    fenv.env_sdkroot = "/MacSDK";
    fenv.max_path_limit = lim;
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &darwin_full;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  for (size_t dlim = 1U; dlim <= 4U; ++dlim) {
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::darwin;
    fenv.arch_name = "arm64";
    fenv.env_sdkroot = "/MacSDK";
    fenv.max_dirs_limit = dlim;
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &darwin_full;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  /* Linux glibc & musl fault injection across fail_on_call, max_path_limit, and max_dirs_limit */
  for (size_t fail_at = 1U; fail_at <= 16U; ++fail_at) {
    linux_glibc.fail_on_call = fail_at;
    linux_glibc.call_count = 0U;
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::linux_os;
    fenv.explicit_sysroot = "";
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &linux_glibc;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::io_error || rc == edg_be_driver_error_t::ok);

    linux_musl.fail_on_call = fail_at;
    linux_musl.call_count = 0U;
    fenv.explicit_sysroot = "/alpine";
    fenv.user_data = &linux_musl;
    const edg_be_driver_error_t rc2 = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc2 == edg_be_driver_error_t::io_error || rc2 == edg_be_driver_error_t::ok);
  }
  linux_glibc.fail_on_call = 0U;
  linux_musl.fail_on_call = 0U;
  for (size_t lim = 1U; lim <= 60U; ++lim) {
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::linux_os;
    fenv.max_path_limit = lim;
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &linux_glibc;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  for (size_t dlim = 1U; dlim <= 5U; ++dlim) {
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::linux_os;
    fenv.max_dirs_limit = dlim;
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &linux_glibc;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  /* Windows MSVC & MinGW fault injection + deduplication (when WindowsSdkDir has "include") */
  for (size_t fail_at = 1U; fail_at <= 8U; ++fail_at) {
    win_msvc.fail_on_call = fail_at;
    win_msvc.call_count = 0U;
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::windows_os;
    fenv.env_vc_tools_dir = "C:/VC";
    fenv.env_win_sdk_dir = "C:/WinKit";
    fenv.env_win_sdk_version = "";
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &win_msvc;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::io_error || rc == edg_be_driver_error_t::ok);

    win_mingw.fail_on_call = fail_at;
    win_mingw.call_count = 0U;
    fenv.env_vc_tools_dir = "";
    fenv.explicit_sysroot = "";
    fenv.env_mingw_prefix = "";
    fenv.user_data = &win_mingw;
    const edg_be_driver_error_t rc2 = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc2 == edg_be_driver_error_t::io_error || rc2 == edg_be_driver_error_t::ok);
  }
  win_msvc.fail_on_call = 0U;
  win_mingw.fail_on_call = 0U;
  for (size_t lim = 1U; lim <= 60U; ++lim) {
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::windows_os;
    fenv.env_vc_tools_dir = "C:/VC";
    fenv.env_win_sdk_dir = "C:/WinKit";
    fenv.max_path_limit = lim;
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &win_msvc;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);

    fenv.env_vc_tools_dir = nullptr;
    fenv.env_mingw_prefix = "C:/msys64/ucrt64";
    fenv.user_data = &win_mingw;
    const edg_be_driver_error_t rc2 = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc2 == edg_be_driver_error_t::out_of_memory || rc2 == edg_be_driver_error_t::ok);
  }
  for (size_t dlim = 1U; dlim <= 4U; ++dlim) {
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::windows_os;
    fenv.env_vc_tools_dir = "C:/VC";
    fenv.env_win_sdk_dir = "C:/WinKit";
    fenv.max_dirs_limit = dlim;
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &win_msvc;
    const edg_be_driver_error_t rc = edg_be_sdk_discover(&fenv, &res);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }

  /* Over-capacity limits (>= MAX), Linux without any C++ STL, and empty env_win_sdk_dir */
  {
    mock_fs_t linux_c_only = {{"/usr/include"}, 1U, false, 0U, 0U};
    edg_be_sdk_probe_env_t fenv = {};
    fenv.host_os = edg_host_os_kind_t::linux_os;
    fenv.max_path_limit = 9999U;
    fenv.max_dirs_limit = 9999U;
    fenv.path_exists_fn = &mock_path_exists_cb;
    fenv.user_data = &linux_c_only;
    EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&fenv, &res));
    TEST_ASSERT(res.num_cxx_include_dirs == 0U);

    fenv.host_os = edg_host_os_kind_t::darwin;
    fenv.env_sdkroot = "/MacSDK";
    fenv.user_data = &darwin_full;
    EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&fenv, &res));

    fenv.host_os = edg_host_os_kind_t::windows_os;
    fenv.env_vc_tools_dir = "C:/VC";
    fenv.env_win_sdk_dir = "";
    fenv.user_data = &win_msvc;
    EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&fenv, &res));
  }

  return edg_be_driver_error_t::ok;
}

/**
 * @brief Verifies `@rspfile` expansion, quoting, escaping, cycle guard, and arena limits.
 * @details Writes temporary response files in the working directory and cleans them up.
 * @return `edg_be_driver_error_t::ok` on success.
 */
static edg_be_driver_error_t test_response_files_and_limits(void) noexcept
{
  edg_be_translated_command_t cmd;
  EDG_BE_DRIVER_CHECK(edg_be_translated_command_init(&cmd));

  const char* null_first[1] = {nullptr};
  const char* null_second[2] = {"cpfe", nullptr};
  TEST_ASSERT(edg_be_expand_response_files(1, nullptr, &cmd) == edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_expand_response_files(1, null_first, nullptr) == edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_expand_response_files(0, null_second, &cmd) == edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_expand_response_files(1, null_first, &cmd) == edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_expand_response_files(2, null_second, &cmd) == edg_be_driver_error_t::invalid_argument);

  const char* missing_rsp[2] = {"cpfe", "@nonexistent_edg_rsp_xyz.rsp"};
  TEST_ASSERT(edg_be_expand_response_files(2, missing_rsp, &cmd) ==
              edg_be_driver_error_t::response_file_io_error);

  /* Valid nested @rspfile with comments, single/double quotes, and backslash escapes */
  {
    FILE* inner = std::fopen("edg_test_inner.rsp", "wb");
    TEST_ASSERT(inner != nullptr);
    std::fprintf(inner, "# comment line%c-O2 'single quoted' %cdouble %c%cquoted%c%c%c plain%c_escaped%c%c",
                 kLf, kQt, kBs, kQt, kBs, kQt, kQt, kBs, kCr, kLf);
    TEST_ASSERT(std::fclose(inner) == 0);

    FILE* outer = std::fopen("edg_test_outer.rsp", "wb");
    TEST_ASSERT(outer != nullptr);
    std::fprintf(outer, "-Wall%c@edg_test_inner.rsp%c# trailing comment", kTab, kLf);
    TEST_ASSERT(std::fclose(outer) == 0);

    const char* ok_argv[3] = {"cpfe", "@", "@edg_test_outer.rsp"};
    EDG_BE_DRIVER_CHECK(edg_be_expand_response_files(3, ok_argv, &cmd));
    TEST_ASSERT(cmd.num_expanded_tokens == 7U);
    TEST_ASSERT(std::strcmp(cmd.expanded_tokens[1], "@") == 0);
    TEST_ASSERT(std::strcmp(cmd.expanded_tokens[4], "single quoted") == 0);

    /* Unterminated quote in response file */
    FILE* bad_q = std::fopen("edg_test_badq.rsp", "wb");
    TEST_ASSERT(bad_q != nullptr);
    std::fprintf(bad_q, "%cunterminated", kQt);
    TEST_ASSERT(std::fclose(bad_q) == 0);
    const char* badq_argv[2] = {"cpfe", "@edg_test_badq.rsp"};
    TEST_ASSERT(edg_be_expand_response_files(2, badq_argv, &cmd) ==
                edg_be_driver_error_t::response_file_io_error);

    /* Self-recursive cycle hitting EDG_BE_DRIVER_MAX_RSP_DEPTH */
    FILE* cyc = std::fopen("edg_test_cyc.rsp", "wb");
    TEST_ASSERT(cyc != nullptr);
    std::fputs("@edg_test_cyc.rsp", cyc);
    TEST_ASSERT(std::fclose(cyc) == 0);
    const char* cyc_argv[2] = {"cpfe", "@edg_test_cyc.rsp"};
    TEST_ASSERT(edg_be_expand_response_files(2, cyc_argv, &cmd) ==
                edg_be_driver_error_t::response_file_io_error);

    /* Oversized >16KB response file */
    FILE* big = std::fopen("edg_test_big.rsp", "wb");
    TEST_ASSERT(big != nullptr);
    for (int k = 0; k < 17000; ++k) {
      std::fputc('a', big);
    }
    TEST_ASSERT(std::fclose(big) == 0);
    const char* big_argv[2] = {"cpfe", "@edg_test_big.rsp"};
    TEST_ASSERT(edg_be_expand_response_files(2, big_argv, &cmd) ==
                edg_be_driver_error_t::response_file_io_error);

    /* Oversized >2048-char single token inside response file */
    FILE* bigtok = std::fopen("edg_test_bigtok.rsp", "wb");
    TEST_ASSERT(bigtok != nullptr);
    for (int k = 0; k < 2100; ++k) {
      std::fputc('x', bigtok);
    }
    TEST_ASSERT(std::fclose(bigtok) == 0);
    const char* bigtok_argv[2] = {"cpfe", "@edg_test_bigtok.rsp"};
    TEST_ASSERT(edg_be_expand_response_files(2, bigtok_argv, &cmd) ==
                edg_be_driver_error_t::out_of_memory);

    /* Escape overflow at exact tok_buf boundary (quoted & unquoted) */
    FILE* esc1 = std::fopen("edg_test_esc1.rsp", "wb");
    TEST_ASSERT(esc1 != nullptr);
    for (int k = 0; k < 2047; ++k) {
      std::fputc('y', esc1);
    }
    std::fprintf(esc1, "%cz", kBs);
    TEST_ASSERT(std::fclose(esc1) == 0);
    const char* esc1_argv[2] = {"cpfe", "@edg_test_esc1.rsp"};
    TEST_ASSERT(edg_be_expand_response_files(2, esc1_argv, &cmd) ==
                edg_be_driver_error_t::out_of_memory);

    FILE* esc2 = std::fopen("edg_test_esc2.rsp", "wb");
    TEST_ASSERT(esc2 != nullptr);
    std::fputc(kQt, esc2);
    for (int k = 0; k < 2047; ++k) {
      std::fputc('y', esc2);
    }
    std::fprintf(esc2, "%c%c%c", kBs, kQt, kQt);
    TEST_ASSERT(std::fclose(esc2) == 0);
    const char* esc2_argv[2] = {"cpfe", "@edg_test_esc2.rsp"};
    TEST_ASSERT(edg_be_expand_response_files(2, esc2_argv, &cmd) ==
                edg_be_driver_error_t::out_of_memory);

    TEST_ASSERT(std::remove("edg_test_inner.rsp") == 0);
    TEST_ASSERT(std::remove("edg_test_outer.rsp") == 0);
    TEST_ASSERT(std::remove("edg_test_badq.rsp") == 0);
    TEST_ASSERT(std::remove("edg_test_cyc.rsp") == 0);
    TEST_ASSERT(std::remove("edg_test_big.rsp") == 0);
    TEST_ASSERT(std::remove("edg_test_bigtok.rsp") == 0);
    TEST_ASSERT(std::remove("edg_test_esc1.rsp") == 0);
    TEST_ASSERT(std::remove("edg_test_esc2.rsp") == 0);

    /* Trailing EOF backslash (both unquoted and quoted) */
    FILE* eof_bs1 = std::fopen("edg_test_eofbs1.rsp", "wb");
    TEST_ASSERT(eof_bs1 != nullptr);
    std::fprintf(eof_bs1, "tok%c", kBs);
    TEST_ASSERT(std::fclose(eof_bs1) == 0);
    const char* eof1_argv[2] = {"cpfe", "@edg_test_eofbs1.rsp"};
    EDG_BE_DRIVER_CHECK(edg_be_expand_response_files(2, eof1_argv, &cmd));
    TEST_ASSERT(std::remove("edg_test_eofbs1.rsp") == 0);

    FILE* eof_bs2 = std::fopen("edg_test_eofbs2.rsp", "wb");
    TEST_ASSERT(eof_bs2 != nullptr);
    std::fprintf(eof_bs2, "%ctok%c", kQt, kBs);
    TEST_ASSERT(std::fclose(eof_bs2) == 0);
    const char* eof2_argv[2] = {"cpfe", "@edg_test_eofbs2.rsp"};
    TEST_ASSERT(edg_be_expand_response_files(2, eof2_argv, &cmd) ==
                edg_be_driver_error_t::response_file_io_error);
    TEST_ASSERT(std::remove("edg_test_eofbs2.rsp") == 0);
  }

  /* Arena / args_limit / list_limit bounds */
  cmd.args_limit = 1U;
  const char* two_args[2] = {"cpfe", "a.c"};
  TEST_ASSERT(edg_be_expand_response_files(2, two_args, &cmd) == edg_be_driver_error_t::out_of_memory);
  EDG_BE_DRIVER_CHECK(edg_be_translated_command_init(&cmd));
  cmd.arena_limit = 2U;
  TEST_ASSERT(edg_be_expand_response_files(1, two_args, &cmd) == edg_be_driver_error_t::out_of_memory);

  return edg_be_driver_error_t::ok;
}

/**
 * @brief Verifies CLI flag translation across all GCC/Clang/EDG options, queries, and error cases.
 * @details Tests C89–C23 / C++98–C++26 standards, stages, flags, SDK flavors, queries, and depfiles.
 * @return `edg_be_driver_error_t::ok` on success.
 */
static edg_be_driver_error_t test_cli_translation_and_depfiles(void) noexcept
{
  edg_be_translated_command_t cmd;
  const char* simple[2] = {"edg++", "main.cpp"};
  TEST_ASSERT(edg_be_translate_command_line(2, simple, nullptr, nullptr) ==
              edg_be_driver_error_t::invalid_argument);

  char win_src_path[32];
  std::snprintf(win_src_path, sizeof(win_src_path), "win%csub%ck.c", kBs, kBs);

  /* Comprehensive GCC/Clang flag sweep */
  const char* rich_argv[] = {
    "edg++", "-std=gnu++20", "-c", "-S", "-emit-llvm", "-shared", "-static",
    "-pie", "-no-pie", "-pipe", "-o", "out.o",
    "-Iinc1", "-I", "inc2", "-isystem", "/sys1", "-isystem=/sys2",
    "-iquote", "qdir", "-idirafter", "adir", "-isysroot", "/mysdk",
    "--sysroot=/mysdk2", "-iprefix", "/pre", "-iwithprefix", "/wpre",
    "-DFOO=1", "-D", "BAR", "-UBAZ", "-include", "pre.h", "-imacros", "mac.h",
    "-MD", "-MMD", "-MP", "-MG", "-MF", "deps.d", "-MT", "t1.o", "-MQ", "t2.o",
    "-O", "-O0", "-O1", "-O2", "-O3", "-Os", "-Oz", "-Ofast", "-Og",
    "-gcodeview", "-gsplit-dwarf", "-gsplit-dwarf=my.dwo", "-gdwarf-4",
    "-g", "-g0", "-g1", "-g2", "-g3", "-ggdb", "-ggdb3",
    "-target", "aarch64-linux-gnu", "--target=x86_64-linux-gnu",
    "-march=x86-64-v3", "-mcpu=apple-m2", "-mtune=generic", "-mattr=+avx2",
    "-mcmodel=small", "-mabi=lp64", "-mfpu=neon", "-m64", "-m32",
    "-pthread", "-rdynamic", "-nostdlib", "-nodefaultlibs", "-nostartfiles",
    "-L/lib1", "-L", "/lib2", "-lm", "-l", "pthread",
    "-Wl,-rpath,/r1,-rpath=/r2,--as-needed", "-Xlinker", "--gc-sections",
    "-framework", "CoreFoundation", "-F/System/Library/Frameworks",
    "-w", "-Wall", "-Wextra", "-pedantic", "-pedantic-errors",
    "-fPIC", "-fpic", "-fPIE", "-fpie", "-fno-pic", "-fno-PIC", "-fno-pie", "-fno-PIE",
    "-fexceptions", "-fno-exceptions", "-frtti", "-fno-rtti",
    "-funwind-tables", "-fasynchronous-unwind-tables", "-fno-unwind-tables",
    "-fopenmp", "-fopenmp-targets=nvptx64-nvidia-cuda",
    "-flto", "-flto=full", "-flto=thin", "-fno-lto",
    "-fvisibility=default", "-fvisibility=hidden", "-fvisibility=protected", "-fvisibility=internal",
    "-fvisibility-inlines-hidden", "-ffunction-sections", "-fdata-sections",
    "-fomit-frame-pointer", "-fno-omit-frame-pointer",
    "-fsigned-char", "-funsigned-char", "-ffast-math",
    "-fsanitize=address,undefined,thread,memory", "-fno-strict-aliasing",
    "-x", "c++", "src1.c", "-x", "none",
    "a.cpp", "b.cc", "c.cxx", "d.cp", "e.CPP", "f.c++", "g.C", "h.ii", "i.ixx", "j.cppm",
    win_src_path, "noext", "-", "libfoo.a", "obj1.o", "obj2.obj", "s.so", "d.dylib", "w.lib"
  };
  const int rich_argc = static_cast<int>(sizeof(rich_argv) / sizeof(rich_argv[0]));
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(rich_argc, rich_argv, nullptr, &cmd));
  TEST_ASSERT(cmd.options.language_is_cplusplus);
  TEST_ASSERT(cmd.options.num_input_object_files == 6U);
  TEST_ASSERT(cmd.options.num_rpath_dirs == 2U);
  TEST_ASSERT(cmd.options.sanitize_address && cmd.options.sanitize_undefined &&
              cmd.options.sanitize_thread && cmd.options.sanitize_memory);

  /* Exercise remaining standards, stages (-E, -fsyntax-only, -M, -MM, -nostdinc*), and native EDG flags */
  const char* all_stds[] = {
    "c89", "c90", "iso9899:1990", "gnu89", "gnu90", "c99", "gnu99",
    "c11", "gnu11", "c17", "c18", "gnu17", "gnu18", "c2x", "c23", "gnu2x", "gnu23",
    "c++98", "c++03", "gnu++98", "gnu++03", "c++11", "gnu++11", "c++14", "gnu++14",
    "c++17", "gnu++17", "c++2a", "c++20", "gnu++2a", "gnu++20",
    "c++2b", "c++23", "gnu++2b", "gnu++23", "c++2c", "c++26", "gnu++2c", "gnu++26"
  };
  for (size_t s = 0; s < sizeof(all_stds) / sizeof(all_stds[0]); ++s) {
    char std_arg[32];
    std::snprintf(std_arg, sizeof(std_arg), "-std=%s", all_stds[s]);
    const char* sargv[3] = {"cpfe", std_arg, "t.c"};
    EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, sargv, nullptr, &cmd));
  }

  const char* stages1[] = {"cpfe", "-E", "-nostdinc", "-nostdinc++", "-nobuiltininc", "-x", "c", "t.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(8, stages1, nullptr, &cmd));
  const char* stages2[] = {"cpfe", "-fsyntax-only", "-M", "-MM", "-x", "assembler-with-cpp", "t.S"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(7, stages2, nullptr, &cmd));
  const char* stages3[] = {"cpfe", "-emit-llvm", "-gcodeview", "-gdwarf-5", "-gsplit-dwarf", "t.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(6, stages3, nullptr, &cmd));

  /* Backend selection & conflicts */
  const char* be1[] = {"cpfe", "--backend=llvm", "--c", "--clear_flag=use_predefined_macro_file",
                       "--gen_llvm_file_name=out.ll", "t.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(6, be1, nullptr, &cmd));
  const char* be2[] = {"cpfe", "--backend=gcc", "--c++20", "--fe_only", "t.cpp"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(5, be2, nullptr, &cmd));
  const char* be3[] = {"cpfe", "--backend=c", "--c-be", "--dump_configuration"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(4, be3, nullptr, &cmd));
  const char* be_bad1[] = {"cpfe", "--backend=invalid"};
  TEST_ASSERT(edg_be_translate_command_line(2, be_bad1, nullptr, &cmd) ==
              edg_be_driver_error_t::conflicting_backend_flags);
  const char* be_bad2[] = {"cpfe", "--gcc-be", "--llvm-be"};
  TEST_ASSERT(edg_be_translate_command_line(3, be_bad2, nullptr, &cmd) ==
              edg_be_driver_error_t::conflicting_backend_flags);
  const char* be_bad3[] = {"cpfe", "--llvm-be", "--gcc-be"};
  TEST_ASSERT(edg_be_translate_command_line(3, be_bad3, nullptr, &cmd) ==
              edg_be_driver_error_t::conflicting_backend_flags);

  /* Invalid / missing CLI flag error paths */
  const char* err_std[] = {"cpfe", "-std=fortran95"};
  TEST_ASSERT(edg_be_translate_command_line(2, err_std, nullptr, &cmd) ==
              edg_be_driver_error_t::unrecognized_cli_flag);
  const char* err_x[] = {"cpfe", "-x", "rust"};
  TEST_ASSERT(edg_be_translate_command_line(3, err_x, nullptr, &cmd) ==
              edg_be_driver_error_t::unrecognized_cli_flag);
  const char* err_o[] = {"cpfe", "-O9"};
  TEST_ASSERT(edg_be_translate_command_line(2, err_o, nullptr, &cmd) ==
              edg_be_driver_error_t::unrecognized_cli_flag);
  const char* err_vis[] = {"cpfe", "-fvisibility=bogus"};
  TEST_ASSERT(edg_be_translate_command_line(2, err_vis, nullptr, &cmd) ==
              edg_be_driver_error_t::unrecognized_cli_flag);
  const char* err_unk[] = {"cpfe", "-Zunknown"};
  TEST_ASSERT(edg_be_translate_command_line(2, err_unk, nullptr, &cmd) ==
              edg_be_driver_error_t::unrecognized_cli_flag);
  const char* err_miss1[] = {"cpfe", "-I"};
  TEST_ASSERT(edg_be_translate_command_line(2, err_miss1, nullptr, &cmd) ==
              edg_be_driver_error_t::missing_cli_argument);
  const char* err_miss2[] = {"cpfe", "-isystem="};
  TEST_ASSERT(edg_be_translate_command_line(2, err_miss2, nullptr, &cmd) ==
              edg_be_driver_error_t::missing_cli_argument);
  const char* err_miss3[] = {"cpfe", "--target="};
  TEST_ASSERT(edg_be_translate_command_line(2, err_miss3, nullptr, &cmd) ==
              edg_be_driver_error_t::missing_cli_argument);
  const char* err_miss4[] = {"cpfe", "-Wl,"};
  TEST_ASSERT(edg_be_translate_command_line(2, err_miss4, nullptr, &cmd) ==
              edg_be_driver_error_t::missing_cli_argument);
  const char* err_miss5[] = {"cpfe", "-fsanitize="};
  TEST_ASSERT(edg_be_translate_command_line(2, err_miss5, nullptr, &cmd) ==
              edg_be_driver_error_t::missing_cli_argument);

  /* Auto-SDK injection across clang_gnu, gcc_gnu (C and C++), and msvc_cl */
  mock_fs_t mac_fs = {{"/MacSDK", "/MacSDK/usr/include/c++/v1", "/MacSDK/usr/include"}, 3U, false};
  edg_be_sdk_probe_env_t senv = {};
  senv.host_os = edg_host_os_kind_t::darwin;
  senv.arch_name = "arm64";
  senv.env_sdkroot = "/MacSDK";
  senv.path_exists_fn = &mock_path_exists_cb;
  senv.user_data = &mac_fs;
  const char* mac_cxx[] = {"edg++", "--sysroot=/MacSDK", "app.cpp"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, mac_cxx, &senv, &cmd));
  TEST_ASSERT(cmd.sdk_discovered);

  mock_fs_t lin_fs = {{"/usr/include", "/usr/include/c++/14"}, 2U, false};
  senv.host_os = edg_host_os_kind_t::linux_os;
  senv.user_data = &lin_fs;
  const char* lin_c[] = {"cpfe", "app.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, lin_c, &senv, &cmd));
  const char* lin_cxx[] = {"edg++", "app.cpp"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, lin_cxx, &senv, &cmd));

  mock_fs_t msvc_fs = {{"C:/VC/include"}, 1U, false};
  senv.host_os = edg_host_os_kind_t::windows_os;
  senv.env_vc_tools_dir = "C:/VC";
  senv.user_data = &msvc_fs;
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, lin_c, &senv, &cmd));

  /* Introspection queries */
  FILE* qout = std::fopen("edg_test_queries.txt", "wb");
  TEST_ASSERT(qout != nullptr);
  TEST_ASSERT(edg_be_execute_driver_query(nullptr, nullptr, qout) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_execute_driver_query(&cmd.options, nullptr, nullptr) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_execute_driver_query(&cmd.options, nullptr, qout) ==
              edg_be_driver_error_t::invalid_argument);

  const char* query_flags[] = {
    "--version", "-v", "-dumpmachine", "-dumpversion", "-dumpfullversion",
    "-print-search-dirs", "-print-prog-name=ld", "-print-file-name=libc.a",
    "-print-libgcc-file-name", "-print-resource-dir"
  };
  for (size_t q = 0; q < sizeof(query_flags) / sizeof(query_flags[0]); ++q) {
    const char* qargv[2] = {"cpfe", query_flags[q]};
    EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, qargv, &senv, &cmd));
    EDG_BE_DRIVER_CHECK(edg_be_execute_driver_query(&cmd.options, &cmd.sdk_info, qout));
    EDG_BE_DRIVER_CHECK(edg_be_execute_driver_query(&cmd.options, nullptr, qout));
  }
  const char* q_with_triple[3] = {"cpfe", "--target=riscv64-linux-gnu", "-dumpmachine"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, q_with_triple, nullptr, &cmd));
  cmd.options.query_argument = nullptr;
  EDG_BE_DRIVER_CHECK(edg_be_execute_driver_query(&cmd.options, nullptr, qout));
  TEST_ASSERT(std::fclose(qout) == 0);
  TEST_ASSERT(std::remove("edg_test_queries.txt") == 0);

  /* Read-only stream passed to edg_be_execute_driver_query triggers io_error */
  FILE* ro_tmp = std::fopen("edg_test_ro.txt", "wb");
  TEST_ASSERT(ro_tmp != nullptr);
  TEST_ASSERT(std::fclose(ro_tmp) == 0);
  FILE* ro_in = std::fopen("edg_test_ro.txt", "rb");
  TEST_ASSERT(ro_in != nullptr);
  TEST_ASSERT(edg_be_execute_driver_query(&cmd.options, nullptr, ro_in) ==
              edg_be_driver_error_t::io_error);
  TEST_ASSERT(std::fclose(ro_in) == 0);
  TEST_ASSERT(std::remove("edg_test_ro.txt") == 0);

  /* CWD object name derivation */
  char obj_buf[64];
  TEST_ASSERT(edg_be_derive_cwd_object_name(nullptr, false, obj_buf, sizeof(obj_buf)) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_derive_cwd_object_name("a.c", false, nullptr, sizeof(obj_buf)) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_derive_cwd_object_name("", false, obj_buf, sizeof(obj_buf)) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_derive_cwd_object_name("/trailing/slash/", false, obj_buf, sizeof(obj_buf)) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_derive_cwd_object_name("/ro/src/my_module.cpp", false, obj_buf, 4U) ==
              edg_be_driver_error_t::out_of_memory);
  EDG_BE_DRIVER_CHECK(edg_be_derive_cwd_object_name("/ro/src/my_module.cpp", false, obj_buf, sizeof(obj_buf)));
  TEST_ASSERT(std::strcmp(obj_buf, "my_module.o") == 0);
  char win_hidden[32];
  std::snprintf(win_hidden, sizeof(win_hidden), "C:%csrc%c.hidden", kBs, kBs);
  EDG_BE_DRIVER_CHECK(edg_be_derive_cwd_object_name(win_hidden, true, obj_buf, sizeof(obj_buf)));
  TEST_ASSERT(std::strcmp(obj_buf, ".hidden.obj") == 0);

  /* Make dependency file writer (-MD, -MF, -MT, -MQ, -MP, escaping, and errors) */
  TEST_ASSERT(edg_be_write_depfile(nullptr, "a.c", nullptr, 0U) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_write_depfile(&cmd, nullptr, nullptr, 0U) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_write_depfile(&cmd, "a.c", nullptr, 2U) ==
              edg_be_driver_error_t::invalid_argument);
  /* Not enabled -> no-op ok */
  EDG_BE_DRIVER_CHECK(edg_be_write_depfile(&cmd, "a.c", nullptr, 0U));

  const char* dep_argv1[] = {
    "cpfe", "-MD", "-MP", "-MF", "edg_test_custom.d",
    "-MT", "dir with space/out#1.o", "-MQ", "$(OBJ)/prequoted.o", "src$1.c"
  };
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(10, dep_argv1, nullptr, &cmd));
  const char* hdrs[3] = {"inc/h 1#a$b.h", "", nullptr};
  EDG_BE_DRIVER_CHECK(edg_be_write_depfile(&cmd, "src$1.c", hdrs, 3U));
  TEST_ASSERT(std::remove("edg_test_custom.d") == 0);

  /* Default .d path derived from -o and from source stem */
  const char* dep_argv2[] = {"cpfe", "-MMD", "-o", "edg_test_out.o", "sub/unit.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(5, dep_argv2, nullptr, &cmd));
  EDG_BE_DRIVER_CHECK(edg_be_write_depfile(&cmd, "sub/unit.c", nullptr, 0U));
  TEST_ASSERT(std::remove("edg_test_out.d") == 0);

  const char* dep_argv3[] = {"cpfe", "-MMD", "sub/plain_no_ext"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, dep_argv3, nullptr, &cmd));
  EDG_BE_DRIVER_CHECK(edg_be_write_depfile(&cmd, "sub/plain_no_ext", nullptr, 0U));
  TEST_ASSERT(std::remove("plain_no_ext.d") == 0);

  /* Oversized -o stem when deriving .d */
  char long_out[EDG_BE_SDK_MAX_PATH_LEN];
  std::memset(long_out, 'z', sizeof(long_out) - 1U);
  long_out[sizeof(long_out) - 1U] = '\0';
  cmd.options.output_file_path = long_out;
  TEST_ASSERT(edg_be_write_depfile(&cmd, "a.c", nullptr, 0U) == edg_be_driver_error_t::out_of_memory);

  /* Unwritable depfile path */
  cmd.options.output_file_path = nullptr;
  cmd.options.dep_file_path = "/definitely_nonexistent_dir_xyz/out.d";
  TEST_ASSERT(edg_be_write_depfile(&cmd, "a.c", nullptr, 0U) ==
              edg_be_driver_error_t::depfile_write_failed);

  /* Exhaustive edg_be_format_depfile_buffer validation and byte-by-byte OOM sweep */
  char fmt_buf[256];
  size_t fmt_len = 0U;
  TEST_ASSERT(edg_be_format_depfile_buffer(nullptr, "a.c", nullptr, 0U, fmt_buf, sizeof(fmt_buf), &fmt_len) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_format_depfile_buffer(&cmd, nullptr, nullptr, 0U, fmt_buf, sizeof(fmt_buf), &fmt_len) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_format_depfile_buffer(&cmd, "a.c", nullptr, 0U, nullptr, sizeof(fmt_buf), &fmt_len) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_format_depfile_buffer(&cmd, "a.c", nullptr, 0U, fmt_buf, sizeof(fmt_buf), nullptr) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_format_depfile_buffer(&cmd, "a.c", nullptr, 1U, fmt_buf, sizeof(fmt_buf), &fmt_len) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_format_depfile_buffer(&cmd, "", nullptr, 0U, fmt_buf, sizeof(fmt_buf), &fmt_len) ==
              edg_be_driver_error_t::invalid_argument);

  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(10, dep_argv1, nullptr, &cmd));
  for (size_t cap = 0U; cap <= 120U; ++cap) {
    const edg_be_driver_error_t rc =
        edg_be_format_depfile_buffer(&cmd, "src$1.c", hdrs, 3U, fmt_buf, cap, &fmt_len);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(5, dep_argv2, nullptr, &cmd));
  for (size_t cap = 1U; cap <= 40U; ++cap) {
    const edg_be_driver_error_t rc =
        edg_be_format_depfile_buffer(&cmd, "sub/unit.c", nullptr, 0U, fmt_buf, cap, &fmt_len);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }

  /* Step-exhaustion fault injection on edg_be_translate_command_line_bounded */
  for (size_t alim = 1U; alim <= 95U; ++alim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc =
        edg_be_translate_command_line_bounded(rich_argc, rich_argv, &senv, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  for (size_t blim = 1U; blim <= 3000U; blim += 2U) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc =
        edg_be_translate_command_line_bounded(rich_argc, rich_argv, &senv, blim, 0U, 0U, &bcmd);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  for (size_t blim = 1U; blim <= 3000U; blim += 1U) {
    if (blim >= 1350U && blim <= 1650U) {
      edg_be_translated_command_t bcmd;
      const edg_be_driver_error_t rc =
          edg_be_translate_command_line_bounded(rich_argc, rich_argv, &senv, blim, 0U, 0U, &bcmd);
      TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
    }
  }
  for (size_t llim = 1U; llim <= 14U; ++llim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc =
        edg_be_translate_command_line_bounded(rich_argc, rich_argv, &senv, 0U, 0U, llim, &bcmd);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  /* Over-capacity limits (>= MAX) on bounded translation */
  {
    edg_be_translated_command_t bcmd;
    EDG_BE_DRIVER_CHECK(edg_be_translate_command_line_bounded(
        2, simple, nullptr,
        EDG_BE_DRIVER_ARENA_CAPACITY + 1U,
        EDG_BE_DRIVER_MAX_ARGS + 1U,
        EDG_BE_DRIVER_MAX_LIST_ITEMS + 1U,
        &bcmd));
  }

  /* Missing-argument check on EVERY flag that consumes a joined or following value */
  const char* flags_needing_arg[] = {
    "--backend", "--sysroot", "-o", "-x", "-I", "-isystem", "-iquote",
    "-idirafter", "-isysroot", "-iprefix", "-iwithprefix", "-D", "-U",
    "-include", "-imacros", "-MF", "-MT", "-MQ", "-target",
    "-L", "-l", "-Xlinker", "-framework", "-F",
    "-arch", "-arch=", "-resource-dir", "-resource-dir=",
    "-weak_framework", "-install_name", "-compatibility_version",
    "-current_version", "-undefined", "-force_load", "-bundle_loader",
    "-exported_symbols_list", "-unexported_symbols_list"
  };
  for (size_t k = 0; k < sizeof(flags_needing_arg) / sizeof(flags_needing_arg[0]); ++k) {
    const char* margv[2] = {"cpfe", flags_needing_arg[k]};
    TEST_ASSERT(edg_be_translate_command_line(2, margv, nullptr, &cmd) ==
                edg_be_driver_error_t::missing_cli_argument);
  }
  /* Also test list_limit = 1 failure on -MT, -m64, -L, -l, -Wl,-rpath, -Xlinker, -framework, -F */
  const char* list_flag_pairs[][3] = {
    {"cpfe", "-MT", "a.o"},
    {"cpfe", "-m64", "-m32"},
    {"cpfe", "-L/a", "-L/b"},
    {"cpfe", "-la", "-lb"},
    {"cpfe", "-Wl,-rpath,/a", "-Wl,-rpath=/b"},
    {"cpfe", "-Xlinker", "a"},
    {"cpfe", "-framework", "CoreFoundation"},
    {"cpfe", "-F/a", "-F/b"}
  };
  for (size_t k = 0; k < sizeof(list_flag_pairs) / sizeof(list_flag_pairs[0]); ++k) {
    for (size_t llim = 1U; llim <= 3U; ++llim) {
      edg_be_translated_command_t bcmd;
      const char* pargv[4] = {
        list_flag_pairs[k][0], list_flag_pairs[k][1], list_flag_pairs[k][2], "u.c"
      };
      const edg_be_driver_error_t rc =
          edg_be_translate_command_line_bounded(4, pargv, nullptr, 0U, 0U, llim, &bcmd);
      TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
    }
    for (size_t blim = 10U; blim <= 45U; ++blim) {
      edg_be_translated_command_t bcmd;
      const edg_be_driver_error_t rc =
          edg_be_translate_command_line_bounded(3, list_flag_pairs[k], nullptr, blim, 0U, 0U, &bcmd);
      TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
    }
  }

  /* Empty positional path "" -> invalid_argument */
  const char* empty_pos[2] = {"cpfe", ""};
  TEST_ASSERT(edg_be_translate_command_line(2, empty_pos, nullptr, &cmd) ==
              edg_be_driver_error_t::invalid_argument);

  /* Also exercise mac_cxx & lin_cxx SDK injection OOM and remaining flag edge cases */
  senv.host_os = edg_host_os_kind_t::darwin;
  senv.user_data = &mac_fs;
  for (size_t alim = 1U; alim <= 14U; ++alim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc =
        edg_be_translate_command_line_bounded(3, mac_cxx, &senv, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);

    const edg_be_driver_error_t rc_c =
        edg_be_translate_command_line_bounded(2, lin_c, nullptr, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc_c == edg_be_driver_error_t::out_of_memory || rc_c == edg_be_driver_error_t::ok);
  }
  const char* native_with_senv[] = {"edg++", "--clear_flag=use_predefined_macro_file", "app.cpp"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, native_with_senv, &senv, &cmd));
  const char* nostdinc_with_senv[] = {"cpfe", "-nostdinc", "app.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, nostdinc_with_senv, &senv, &cmd));
  const char* nostdincxx_with_senv[] = {"edg++", "-nostdinc++", "app.cpp"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, nostdincxx_with_senv, &senv, &cmd));
  const char* explicit_emu_with_senv[] = {"cpfe", "--clang", "--gcc", "--g++", "--microsoft", "app.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(6, explicit_emu_with_senv, &senv, &cmd));
  const char* no_inputs[] = {"cpfe"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(1, no_inputs, nullptr, &cmd));
  const char* verbose_obj[] = {"cpfe", "-v", "obj.o"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, verbose_obj, nullptr, &cmd));
  const char* verbose_ver[] = {"cpfe", "-v", "--version"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, verbose_ver, nullptr, &cmd));

  mock_fs_t fail_mac = {{}, 0U, true, 0U, 0U};
  senv.user_data = &fail_mac;
  TEST_ASSERT(edg_be_translate_command_line(3, mac_cxx, &senv, &cmd) ==
              edg_be_driver_error_t::io_error);

  /* Remaining dialect/query/flag branches: --backendx, --sysrootx, -fPIC -shared, -g -gcodeview, -gsplit-dwarf first */
  const char* extra_edg[] = {
    "a", "--c89", "--c99", "--c11", "--c17", "--c23",
    "--backendx", "--sysrootx", "--llvm_gen_be", "--c_gen_be", "--backend", "llvm",
    "--sysroot", "/tmp", "-Wl,-rpath=",
    "-fPIC", "-shared", "-gsplit-dwarf", "-g", "-gcodeview", "-gdwarf-2", "-gdwarf-3",
    "-fsanitize=unknown_extremely_long_sanitizer_name_exceeding_buffer,custom",
    "-x", "c-header", "-x", "cpp-output",
    "-x", "c++-header", "-x", "c++-cpp-output", "-v", "unit.c"
  };
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(
      static_cast<int>(sizeof(extra_edg) / sizeof(extra_edg[0])), extra_edg, nullptr, &cmd));

  const char* gdwarf_first[] = {"cpfe", "-gdwarf-4", "unit.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, gdwarf_first, nullptr, &cmd));

  /* Query edge cases: empty target_triple, empty sdk_info strings, invalid query enum */
  edg_be_sdk_result_t empty_sdk = {};
  cmd.options.query_kind = edg_driver_query_kind_t::print_search_dirs;
  cmd.options.target_triple = "";
  FILE* qtmp = std::fopen("edg_test_q2.txt", "wb");
  TEST_ASSERT(qtmp != nullptr);
  EDG_BE_DRIVER_CHECK(edg_be_execute_driver_query(&cmd.options, &empty_sdk, qtmp));
  std::snprintf(empty_sdk.detected_resource_dir, sizeof(empty_sdk.detected_resource_dir),
                "/opt/clang/21/include");
  EDG_BE_DRIVER_CHECK(edg_be_execute_driver_query(&cmd.options, &empty_sdk, qtmp));
  cmd.options.query_kind = edg_driver_query_kind_t::print_prog_name;
  cmd.options.query_argument = nullptr;
  EDG_BE_DRIVER_CHECK(edg_be_execute_driver_query(&cmd.options, &empty_sdk, qtmp));
  cmd.options.query_kind = static_cast<edg_driver_query_kind_t>(99);
  TEST_ASSERT(edg_be_execute_driver_query(&cmd.options, &empty_sdk, qtmp) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(std::fclose(qtmp) == 0);

  /* Depfile stream errors (nullptr stream, read-only stream, empty -MF / empty -o, bad source) */
  const char* dep_empty_opts[] = {"cpfe", "-MD", "unit.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, dep_empty_opts, nullptr, &cmd));
  cmd.options.dep_file_path = "";
  cmd.options.output_file_path = "";
  EDG_BE_DRIVER_CHECK(edg_be_write_depfile(&cmd, "unit.c", nullptr, 0U));
  TEST_ASSERT(std::remove("unit.d") == 0);
  TEST_ASSERT(edg_be_write_depfile(&cmd, "", nullptr, 0U) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_write_depfile_stream(&cmd, "unit.c", nullptr, 0U, nullptr) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_write_depfile_stream(&cmd, "", nullptr, 0U, nullptr) ==
              edg_be_driver_error_t::invalid_argument);
  FILE* ro_dep = std::fopen("edg_test_q2.txt", "rb");
  TEST_ASSERT(ro_dep != nullptr);
  TEST_ASSERT(edg_be_write_depfile_stream(&cmd, "", nullptr, 0U, ro_dep) ==
              edg_be_driver_error_t::invalid_argument);
  TEST_ASSERT(edg_be_write_depfile_stream(&cmd, "unit.c", nullptr, 0U, ro_dep) ==
              edg_be_driver_error_t::depfile_write_failed);
  TEST_ASSERT(std::fclose(ro_dep) == 0);
  TEST_ASSERT(std::remove("edg_test_q2.txt") == 0);

  const char* gcc_alias[] = {"cpfe", "--gcc_gen_be", "--dumpmachine"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, gcc_alias, nullptr, &cmd));
  const char* q_aliases1[] = {"cpfe", "--dumpversion"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, q_aliases1, nullptr, &cmd));
  const char* q_aliases2[] = {"cpfe", "--dumpfullversion"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, q_aliases2, nullptr, &cmd));
  const char* q_aliases3[] = {"cpfe", "--print-search-dirs"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, q_aliases3, nullptr, &cmd));

  /* Stage-2 Darwin loader tokens, -dynamiclib, -ansi, -arch, -resource-dir, ISA & linker flags */
  const char* stage2_flags[] = {
    "cpfe", "-dynamiclib", "-pthread", "-Qunused-arguments", "-P", "-C", "-CC", "-dM", "-stdlib=libc++",
    "-arch", "arm64", "-arch=x86_64", "-resource-dir", "/res1", "-resource-dir=/res2",
    "-mmacosx-version-min=14.0", "-miphoneos-version-min=17.0",
    "-msse4.2", "-mavx2", "-mfma", "-mbmi2", "-mpclmul", "-maes", "-mcrc", "-msha",
    "-mlzcnt", "-mpopcnt",
    "-weak_framework", "CoreAudio",
    "-install_name", "@rpath/libfoo.dylib",
    "-compatibility_version", "1.0", "-current_version", "1.2",
    "-undefined", "dynamic_lookup",
    "-force_load", "@loader_path/libbar.a",
    "-bundle_loader", "@executable_path/host_app",
    "-exported_symbols_list", "exp.txt", "-unexported_symbols_list", "unexp.txt",
    "-headerpad_max_install_names", "-flat_namespace", "-dead_strip",
    "-search_paths_first", "-bundle", "-single_module", "-fuse-ld=lld",
    "unit.c"
  };
  const int stage2_argc = static_cast<int>(sizeof(stage2_flags) / sizeof(stage2_flags[0]));
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(stage2_argc, stage2_flags, nullptr, &cmd));
  for (size_t llim = 1U; llim <= 28U; ++llim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc =
        edg_be_translate_command_line_bounded(stage2_argc, stage2_flags, nullptr, 0U, 0U, llim, &bcmd);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  for (size_t blim = 1U; blim <= 900U; ++blim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc =
        edg_be_translate_command_line_bounded(stage2_argc, stage2_flags, nullptr, blim, 0U, 0U, &bcmd);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }

  const char* ansi_argv[] = {"cpfe", "-ansi", "unit.c"};
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(3, ansi_argv, nullptr, &cmd));
  TEST_ASSERT(!cmd.options.language_is_cplusplus && !cmd.options.gnu_extensions);

  /* C++20 modules flags (-fmodules, -fmodules-ts, -fmodule-output=, -fprebuilt-module-path=, -fmodule-file=) */
  mock_fs_t gnu_fs = {{"/usr/include", "/usr/include/c++/14"}, 2U, false, 0U, 0U};
  edg_be_sdk_probe_env_t aarch_env = {};
  aarch_env.host_os = edg_host_os_kind_t::linux_os;
  aarch_env.arch_name = "aarch64";
  aarch_env.path_exists_fn = &mock_path_exists_cb;
  aarch_env.user_data = &gnu_fs;

  const char* mod_implicit[] = {
    "edg++", "-fmodules", "-fmodules-ts",
    "-fmodule-output=m.pcm", "-fprebuilt-module-path=mods", "-fmodule-file=m=m.pcm",
    "mod.cppm"
  };
  const int mod_argc = static_cast<int>(sizeof(mod_implicit) / sizeof(mod_implicit[0]));
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(mod_argc, mod_implicit, &aarch_env, &cmd));
  for (size_t alim = 1U; alim <= 25U; ++alim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc =
        edg_be_translate_command_line_bounded(mod_argc, mod_implicit, &aarch_env, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
  }
  const char* const mod_singles[3][3] = {
    {"edg++", "-fmodule-output=m.pcm", "mod.cppm"},
    {"edg++", "-fprebuilt-module-path=mods", "mod.cppm"},
    {"edg++", "-fmodule-file=m=m.pcm", "mod.cppm"}
  };
  for (size_t mi = 0U; mi < 3U; ++mi) {
    for (size_t alim = 1U; alim <= 5U; ++alim) {
      edg_be_translated_command_t bcmd;
      const edg_be_driver_error_t rc =
          edg_be_translate_command_line_bounded(3, mod_singles[mi], nullptr, 0U, alim, 0U, &bcmd);
      TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
    }
    for (size_t blim = 1U; blim <= 90U; ++blim) {
      edg_be_translated_command_t bcmd;
      const edg_be_driver_error_t rc =
          edg_be_translate_command_line_bounded(3, mod_singles[mi], nullptr, blim, 0U, 0U, &bcmd);
      TEST_ASSERT(rc == edg_be_driver_error_t::out_of_memory || rc == edg_be_driver_error_t::ok);
    }
  }

  /* Exercise Darwin, Linux aarch64, and Linux x86_64 GNU predefined macro injection & OOM paths */
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, lin_cxx, &aarch_env, &cmd));

  edg_be_sdk_probe_env_t x86_env = aarch_env;
  x86_env.arch_name = "x86_64";
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(2, lin_cxx, &x86_env, &cmd));

  mock_fs_t dar_fs = {{"/MacSDK", "/MacSDK/usr/include/c++/v1", "/MacSDK/usr/include"}, 3U, false, 0U, 0U};
  edg_be_sdk_probe_env_t dar_env = {};
  dar_env.host_os = edg_host_os_kind_t::darwin;
  dar_env.arch_name = "arm64";
  dar_env.env_sdkroot = "/MacSDK";
  dar_env.path_exists_fn = &mock_path_exists_cb;
  dar_env.user_data = &dar_fs;

  for (size_t alim = 1U; alim <= 320U; ++alim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc_d =
        edg_be_translate_command_line_bounded(2, simple, &dar_env, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc_d == edg_be_driver_error_t::out_of_memory || rc_d == edg_be_driver_error_t::ok);
    const edg_be_driver_error_t rc_a =
        edg_be_translate_command_line_bounded(2, lin_cxx, &aarch_env, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc_a == edg_be_driver_error_t::out_of_memory || rc_a == edg_be_driver_error_t::ok);
    const edg_be_driver_error_t rc_x =
        edg_be_translate_command_line_bounded(2, lin_cxx, &x86_env, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc_x == edg_be_driver_error_t::out_of_memory || rc_x == edg_be_driver_error_t::ok);
  }

  /* Two-token native EDG long options (--edg_base_dir, --fsanitize, --gen_llvm_file_name, --llvm_target) */
  const char* const native_two_tok[] = {
    "cpfe",
    "--c++20",
    "--edg_base_dir",
    "./bases/clang",
    "--fsanitize",
    "address",
    "--llvm_target",
    "x86_64-unknown-linux-gnu",
    "--gen_llvm_file_name",
    "./out.ll",
    "in.cpp"
  };
  EDG_BE_DRIVER_CHECK(edg_be_translate_command_line(11, native_two_tok, &dar_env, &cmd));
  TEST_ASSERT(cmd.native_edg_mode);
  TEST_ASSERT(cmd.options.emit_llvm_ir_file);
  TEST_ASSERT(cmd.options.num_input_source_files == 1U);

  const char* const missing_two_tok[] = {"cpfe", "--edg_base_dir"};
  TEST_ASSERT(edg_be_translate_command_line(2, missing_two_tok, nullptr, &cmd) ==
              edg_be_driver_error_t::missing_cli_argument);

  for (size_t alim = 1U; alim <= 16U; ++alim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc_n =
        edg_be_translate_command_line_bounded(11, native_two_tok, nullptr, 0U, alim, 0U, &bcmd);
    TEST_ASSERT(rc_n == edg_be_driver_error_t::out_of_memory || rc_n == edg_be_driver_error_t::ok);
  }
  for (size_t blim = 1U; blim <= 160U; ++blim) {
    edg_be_translated_command_t bcmd;
    const edg_be_driver_error_t rc_n =
        edg_be_translate_command_line_bounded(11, native_two_tok, nullptr, blim, 0U, 0U, &bcmd);
    TEST_ASSERT(rc_n == edg_be_driver_error_t::out_of_memory || rc_n == edg_be_driver_error_t::ok);
  }

  return edg_be_driver_error_t::ok;
}

/**
 * @brief Entry point executing the complete Stage-0 shared driver & SDK test suite.
 * @details Returns `0` when all assertions and error-propagation checks succeed.
 * @return `0` on pass, `1` on failure.
 */
int main(void)
{
  edg_be_driver_error_t rc = test_error_strings_and_inits();
  if (rc != edg_be_driver_error_t::ok) {
    std::fprintf(stderr, "Suite 1 failed: %d%c", static_cast<int>(rc), kLf);
    return 1;
  }
  rc = test_sdk_discovery_all_platforms();
  if (rc != edg_be_driver_error_t::ok) {
    std::fprintf(stderr, "Suite 2 failed: %d%c", static_cast<int>(rc), kLf);
    return 1;
  }
  rc = test_response_files_and_limits();
  if (rc != edg_be_driver_error_t::ok) {
    std::fprintf(stderr, "Suite 3 failed: %d%c", static_cast<int>(rc), kLf);
    return 1;
  }
  rc = test_cli_translation_and_depfiles();
  if (rc != edg_be_driver_error_t::ok) {
    std::fprintf(stderr, "Suite 4 failed: %d%c", static_cast<int>(rc), kLf);
    return 1;
  }
  std::puts("[PASS] All Stage-0 shared driver and SDK tests succeeded.");
  return 0;
}
