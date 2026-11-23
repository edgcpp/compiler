/**
 * @file edg_be_common.h
 * @brief Shared multi-backend error codes, portability macros, and invocation contracts.
 * @details Defines the centralized nodiscard error enum, error-percolation macros,
 * noexcept attributes, and normalized compiler driver invocation structures shared
 * across the EDG front end and its C, LLVM, and GCC backends.
 * @ingroup edg_be_common
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef EDG_BE_COMMON_H
#define EDG_BE_COMMON_H 1

/**
 * @defgroup edg_be_common Shared Multi-Backend Driver & Invocation Contracts
 * @brief Shared error enumerations, attributes, and normalized CLI option contracts.
 * @details Provides conflict-free Stage-0 interfaces shared between the EDG front end,
 * the LLVM IR backend, and the GCC (`libgccjit`) backend.
 * @{
 */

#include "basic_hdrs.h"
#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/**
 * @def EDG_BE_NODISCARD
 * @brief Marks an error type or function return value as mandatory to inspect.
 * @details Expands to `[[nodiscard]]` in C++17+ (including MSVC via `_MSVC_LANG`),
 * `__attribute__((warn_unused_result))` on GCC/Clang C, and `_Check_return_` on MSVC C.
 */
#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define EDG_BE_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
#define EDG_BE_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define EDG_BE_NODISCARD _Check_return_
#else
#define EDG_BE_NODISCARD
#endif

/**
 * @def EDG_BE_NOEXCEPT
 * @brief Marks a function declaration or definition as non-throwing.
 * @details Expands to `noexcept` when compiled as C++11 or later (checking both
 * `__cplusplus` and `_MSVC_LANG` for MSVC without `/Zc:__cplusplus`), and empty in C.
 */
#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#define EDG_BE_NOEXCEPT noexcept
#else
#define EDG_BE_NOEXCEPT
#endif

BEGIN_EDG_NAMESPACE

#ifdef __cplusplus
/**
 * @enum edg_be_driver_error_t
 * @brief Centralized error status codes for the shared multi-backend driver and SDK layer.
 * @details Every function in `edg_be_driver` and `edg_be_sdk` returns this `[[nodiscard]]`
 * enumeration and writes computed values via validated out-parameters.
 */
enum class [[nodiscard]] edg_be_driver_error_t : int32_t {
  ok = 0,                          ///< Operation completed successfully.
  invalid_argument = 1,            ///< Null pointer or invalid parameter passed to driver API.
  out_of_memory = 2,               ///< Dynamic allocation with std::nothrow or malloc failed.
  unrecognized_cli_flag = 3,       ///< Unknown or malformed command-line flag.
  missing_cli_argument = 4,        ///< Flag requiring an argument was supplied without one.
  conflicting_backend_flags = 5,   ///< Mutually incompatible backend or output-mode flags.
  backend_not_compiled_in = 6,     ///< Requested backend (llvm, gcc, or c) was disabled at build time.
  sdk_discovery_failed = 7,        ///< Host sysroot, C runtime headers, or C++ STL paths could not be resolved.
  response_file_io_error = 8,      ///< Failed to open or parse @rspfile response file.
  depfile_write_failed = 9,        ///< Failed to open, format, or flush Make dependency file.
  temp_file_creation_failed = 10,  ///< Failed to create per-invocation temporary directory or artifact path.
  frontend_compilation_failed = 11,///< EDG front-end parsing or semantic analysis reported errors on a TU.
  llvm_backend_failed = 12,        ///< Dispatched LLVM backend returned a non-ok llvm_gen_be_error_t.
  gcc_backend_failed = 13,         ///< Dispatched GCC backend returned a non-success gcc_gen_be_error_t.
  c_backend_failed = 14,           ///< Dispatched C-generating backend reported an error.
  linker_execution_failed = 15,    ///< External or integrated linker invocation failed.
  io_error = 16                    ///< Filesystem stat, read, write, or rename error.
};

/**
 * @def EDG_BE_DRIVER_CHECK
 * @brief Evaluates a driver expression once and percolates any non-ok error immediately.
 * @details Prevents duplicate evaluation and enforces zero-(void)-ignore error propagation.
 * @param expr Expression returning `edg_be_driver_error_t`.
 */
#define EDG_BE_DRIVER_CHECK(expr) do { const edg_be_driver_error_t _edg_drv_status = (expr); if (_edg_drv_status != edg_be_driver_error_t::ok) { return _edg_drv_status; } } while (false)

/**
 * @def EDG_BE_DRIVER_CHECK_NOT_NULL
 * @brief Validates that a pointer argument is non-null, returning `invalid_argument` otherwise.
 * @details Evaluates `ptr` once and returns `edg_be_driver_error_t::invalid_argument` if null.
 * @param ptr Pointer expression to validate against `nullptr`.
 */
#define EDG_BE_DRIVER_CHECK_NOT_NULL(ptr) do { if ((ptr) == nullptr) { return edg_be_driver_error_t::invalid_argument; } } while (false)

/**
 * @enum edg_backend_kind_t
 * @brief Identifies which code-generation backend was selected for the invocation.
 * @details Used by the front-end driver router to dispatch `llvm_gen_be`, `gcc_gen_be`, or `c_gen_be`.
 */
enum class edg_backend_kind_t : uint8_t {
  auto_select = 0, ///< Select default compiled-in backend automatically.
  llvm_be = 1,     ///< Native LLVM IR / MC code generation backend.
  gcc_be = 2,      ///< Native GCC (libgccjit) code generation backend.
  c_gen_be = 3,    ///< Portable C source-generating backend.
  fe_only = 4      ///< Front-end syntax/semantic analysis or preprocessing only.
};

/**
 * @enum edg_output_artifact_kind_t
 * @brief Identifies the primary compilation artifact requested on the command line.
 * @details Normalized from `-c`, `-S`, `-E`, `-shared`, `-emit-llvm`, and `-fsyntax-only`.
 */
enum class edg_output_artifact_kind_t : uint8_t {
  executable = 0,          ///< Linked native executable binary (default).
  shared_library = 1,      ///< Linked dynamic shared library (-shared).
  object_file = 2,         ///< Relocatable machine object file (-c).
  assembly_file = 3,       ///< Native textual assembly file (-S).
  ir_or_c_file = 4,        ///< Intermediate representation (-emit-llvm / --c-be).
  preprocessed_output = 5, ///< Preprocessed source text (-E).
  syntax_only = 6          ///< Syntax and semantic verification only (-fsyntax-only).
};

/**
 * @enum edg_reloc_mode_t
 * @brief Relocation model requested for machine code generation.
 * @details Normalized from `-fPIC`, `-fpic`, `-fPIE`, `-fpie`, `-fno-pic`, and `-fno-pie`.
 */
enum class edg_reloc_mode_t : uint8_t {
  default_reloc = 0, ///< Target platform default relocation model.
  static_no_pic = 1, ///< Non-position-independent static relocation (-fno-pic / -fno-pie).
  pic = 2,           ///< Position-independent shared code (-fPIC / -fpic).
  pie = 3            ///< Position-independent executable code (-fPIE / -fpie).
};

/**
 * @enum edg_lto_mode_t
 * @brief Link-Time Optimization mode requested on the command line.
 * @details Normalized from `-flto`, `-flto=full`, `-flto=thin`, and `-fno-lto`.
 */
enum class edg_lto_mode_t : uint8_t {
  none = 0,     ///< Link-time optimization disabled.
  full_lto = 1, ///< Monolithic full link-time optimization (-flto / -flto=full).
  thin_lto = 2  ///< Scalable summary-based ThinLTO (-flto=thin).
};

/**
 * @enum edg_visibility_mode_t
 * @brief Default ELF/Mach-O symbol visibility mode.
 * @details Normalized from `-fvisibility=default|hidden|protected|internal`.
 */
enum class edg_visibility_mode_t : uint8_t {
  default_vis = 0,   ///< Default external symbol visibility.
  hidden_vis = 1,    ///< Hidden symbol visibility (-fvisibility=hidden).
  protected_vis = 2, ///< Protected symbol visibility (-fvisibility=protected).
  internal_vis = 3   ///< Internal symbol visibility (-fvisibility=internal).
};

/**
 * @enum edg_driver_query_kind_t
 * @brief Standalone compiler-driver introspection queries used by build systems.
 * @details Normalized from `--version`, `-dumpmachine`, `-dumpversion`, `-print-search-dirs`, etc.
 */
enum class edg_driver_query_kind_t : uint8_t {
  none = 0,                   ///< Normal compilation invocation (not an immediate query).
  print_version = 1,          ///< Print compiler version banner (--version).
  print_verbose = 2,          ///< Print verbose toolchain configuration (-v with no inputs).
  dump_machine = 3,           ///< Print target machine triple (-dumpmachine).
  dump_version = 4,           ///< Print short compatibility version (-dumpversion).
  dump_full_version = 5,      ///< Print full compatibility version (-dumpfullversion).
  print_search_dirs = 6,      ///< Print program and library search directories (-print-search-dirs).
  print_prog_name = 7,        ///< Print path to requested subprogram (-print-prog-name=<name>).
  print_file_name = 8,        ///< Print path to requested runtime/library file (-print-file-name=<name>).
  print_libgcc_file_name = 9, ///< Print path to runtime support library (-print-libgcc-file-name).
  print_resource_dir = 10     ///< Print compiler builtin resource directory (-print-resource-dir).
};
#endif /* __cplusplus */

/**
 * @struct edg_be_invocation_options_t
 * @brief Normalized backend-agnostic compiler and linker options for a driver invocation.
 * @details Populated once by `edg_be_driver` during CLI translation and passed read-only
 * into `llvm_apply_invocation_options` or `gcc_gen_be_apply_invocation_options` so backend
 * implementations remain decoupled from front-end option parsing internals.
 */
struct edg_be_invocation_options_t {
  edg_backend_kind_t selected_backend;        ///< Selected backend (auto, llvm, gcc, c, or fe_only).
  edg_output_artifact_kind_t artifact_kind;   ///< Primary output artifact kind (-c, -S, -E, -shared, exe).
  edg_reloc_mode_t reloc_mode;                ///< Relocation mode (default, static_no_pic, pic, pie).
  edg_lto_mode_t lto_mode;                    ///< Link-time optimization mode (none, full_lto, thin_lto).
  edg_visibility_mode_t symbol_visibility;    ///< Default symbol visibility mode.
  edg_driver_query_kind_t query_kind;         ///< Immediate build-system query kind, if any.
  const char* query_argument;                 ///< Optional argument to -print-prog-name or -print-file-name.
  bool visibility_inlines_hidden;             ///< True when -fvisibility-inlines-hidden is active.
  uint8_t opt_level;                          ///< Optimization level (0..3).
  uint8_t opt_size_level;                     ///< Size optimization level (0=none, 1=-Os, 2=-Oz).
  bool fast_math;                             ///< True when -Ofast or -ffast-math is enabled.
  uint8_t debug_level;                        ///< Debug info level (0=off, 1=-g1, 2=-g, 3=-g3).
  uint8_t dwarf_version;                      ///< Explicit DWARF version (0=default, or 2..5).
  bool codeview_debug;                        ///< True when -gcodeview debug info is requested.
  const char* split_dwarf_path;               ///< Optional output path for -gsplit-dwarf (.dwo).
  bool exceptions_enabled;                    ///< True when language exceptions (-fexceptions) are enabled.
  bool rtti_enabled;                          ///< True when C++ RTTI (-frtti) is enabled.
  bool unwind_tables;                         ///< True when unwind tables (-funwind-tables) are requested.
  bool openmp_enabled;                        ///< True when OpenMP (-fopenmp) is enabled.
  const char* openmp_targets;                 ///< Optional target triple list from -fopenmp-targets=...
  bool pthread_enabled;                       ///< True when -pthread is passed (defines _REENTRANT & links pthread).
  bool static_link;                           ///< True when -static linking is requested.
  bool rdynamic_enabled;                      ///< True when -rdynamic dynamic symbol export is requested.
  bool nostdlib_link;                         ///< True when -nostdlib is specified.
  bool nodefaultlibs_link;                    ///< True when -nodefaultlibs is specified.
  bool nostartfiles_link;                     ///< True when -nostartfiles is specified.
  bool nostdinc;                              ///< True when -nostdinc disables system C/C++ include discovery.
  bool nostdincxx;                            ///< True when -nostdinc++ disables C++ STL include discovery.
  bool nobuiltininc;                          ///< True when -nobuiltininc disables compiler resource include dir.
  bool function_sections;                     ///< True when -ffunction-sections is enabled.
  bool data_sections;                         ///< True when -fdata-sections is enabled.
  bool omit_frame_pointer;                    ///< True when -fomit-frame-pointer is enabled.
  bool sanitize_address;                      ///< True when -fsanitize=address is enabled.
  bool sanitize_undefined;                    ///< True when -fsanitize=undefined is enabled.
  bool sanitize_thread;                       ///< True when -fsanitize=thread is enabled.
  bool sanitize_memory;                       ///< True when -fsanitize=memory is enabled.
  bool language_is_cplusplus;                 ///< True when compiling or linking in C++ mode (e.g., edg++).
  uint32_t language_std_version;              ///< Normalized ISO standard year/month code (e.g., 202002).
  bool gnu_extensions;                        ///< True when GNU language extensions are enabled.
  bool emit_llvm_ir_file;                     ///< True only when -emit-llvm or --gen_llvm_file_name is requested.
  const char* target_triple;                  ///< Explicit target triple override, or nullptr for host default.
  const char* march;                          ///< Explicit -march=<arch> value, or nullptr.
  const char* mcpu;                           ///< Explicit -mcpu=<cpu> value, or nullptr.
  const char* mtune;                          ///< Explicit -mtune=<cpu> value, or nullptr.
  const char* mattr;                          ///< Comma-separated target feature attributes, or nullptr.
  const char* mcmodel;                        ///< Explicit -mcmodel=<model> value, or nullptr.
  const char* mabi;                           ///< Explicit -mabi=<abi> value, or nullptr.
  const char* mfpu;                           ///< Explicit -mfpu=<fpu> value, or nullptr.
  const char* sysroot;                        ///< Explicit --sysroot / -isysroot path, or nullptr.
  const char* output_file_path;               ///< Explicit -o <path> output file, or nullptr.
  const char* temp_work_dir;                  ///< Per-invocation scratch directory for temporary object files.
  bool generate_depfile;                      ///< True when -M, -MM, -MD, or -MMD dependency generation is active.
  const char* dep_file_path;                  ///< Explicit -MF <file> path, or nullptr to derive from output.
  bool dep_system_headers;                    ///< True when system headers are included (-M / -MD vs -MM / -MMD).
  bool dep_phony_targets;                     ///< True when -MP phony header rules should be emitted.
  const char* const* dep_targets;             ///< Array of explicit -MT / -MQ target strings.
  size_t num_dep_targets;                     ///< Number of entries in `dep_targets`.
  const char* const* input_source_files;      ///< Array of C/C++ source files to compile in this invocation.
  size_t num_input_source_files;              ///< Number of entries in `input_source_files`.
  const char* const* input_object_files;      ///< Array of precompiled .o/.a/.so/.dylib/.lib linker inputs.
  size_t num_input_object_files;              ///< Number of entries in `input_object_files`.
  const char* const* library_search_dirs;     ///< Array of -L<dir> library search paths.
  size_t num_library_search_dirs;             ///< Number of entries in `library_search_dirs`.
  const char* const* libraries;               ///< Array of -l<name> library names.
  size_t num_libraries;                       ///< Number of entries in `libraries`.
  const char* const* linker_passthrough_args; ///< Array of -Wl,* / -Xlinker / -framework linker flags.
  size_t num_linker_passthrough_args;         ///< Number of entries in `linker_passthrough_args`.
  const char* const* rpath_dirs;              ///< Array of runtime library search paths (-Wl,-rpath,*).
  size_t num_rpath_dirs;                      ///< Number of entries in `rpath_dirs`.
  const char* const* extra_backend_args;      ///< Array of backend-specific forwarding options.
  size_t num_extra_backend_args;              ///< Number of entries in `extra_backend_args`.
};

/**
 * @brief Converts an `edg_be_driver_error_t` status code to a static human-readable string.
 * @details Validates `out_str` and writes a non-null static C string describing `err`.
 * @param[in] err Error code enumerator to convert.
 * @param[out] out_str Pointer to receive the static null-terminated description string.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_str` is null.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_driver_error_to_string(
    edg_be_driver_error_t err,
    const char** out_str) EDG_BE_NOEXCEPT;

/**
 * @brief Initializes an `edg_be_invocation_options_t` structure to safe deterministic defaults.
 * @details Zeroes all pointers/counts, sets `debug_level = 0`, `opt_level = 0`, `exceptions_enabled = true`,
 * and `rtti_enabled = true` so callers start from a known baseline.
 * @param[out] out_opts Pointer to the invocation options structure to initialize.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_opts` is null.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_invocation_options_init(
    edg_be_invocation_options_t* out_opts) EDG_BE_NOEXCEPT;

END_EDG_NAMESPACE

/** @} */

#endif /* EDG_BE_COMMON_H */
