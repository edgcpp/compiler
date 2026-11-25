# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.19)

# Standardized backend CMake options (with backward compatibility for ENABLE_LLVM_BACKEND).
if(DEFINED ENABLE_LLVM_BACKEND AND NOT DEFINED EDG_ENABLE_LLVM_BACKEND)
  set(EDG_ENABLE_LLVM_BACKEND "${ENABLE_LLVM_BACKEND}" CACHE BOOL
      "Enable the LLVM IR code generation backend" FORCE)
endif()
option(EDG_ENABLE_LLVM_BACKEND "Enable the LLVM IR code generation backend" OFF)
option(ENABLE_LLVM_BACKEND "Enable the LLVM IR code generation backend (alias)" ${EDG_ENABLE_LLVM_BACKEND})
if(ENABLE_LLVM_BACKEND AND NOT EDG_ENABLE_LLVM_BACKEND)
  set(EDG_ENABLE_LLVM_BACKEND ON CACHE BOOL "Enable the LLVM IR code generation backend" FORCE)
elseif(EDG_ENABLE_LLVM_BACKEND AND NOT ENABLE_LLVM_BACKEND)
  set(ENABLE_LLVM_BACKEND ON CACHE BOOL "Enable the LLVM IR code generation backend (alias)" FORCE)
endif()

option(EDG_ENABLE_GCC_BACKEND "Enable the GCC (libgccjit) code generation backend" OFF)
option(EDG_ENABLE_C_BACKEND "Enable the C/C++ source generating backend" ON)
option(EDG_GCC_BE_ENABLE_COVERAGE "Enable code coverage flags for GCC backend" OFF)
option(EDG_LLVM_BE_COVERAGE "Enable Clang source-based code coverage for LLVM backend tests" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND APPLE AND NOT EDG_ENABLE_LLVM_BACKEND AND NOT EDG_ENABLE_GCC_BACKEND)
  # Release builds on MacOS are built for both the ARM and x86_64
  # architectures.
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

# set the project name
project(cpfe)

# Add our cmake directory to module path for FindLibGCCJIT.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(EDG_ENABLE_GCC_BACKEND)
  find_package(LibGCCJIT REQUIRED)
endif()

# Require that we always have Python available.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Check for attempts to use an in-source build.
if("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  message(FATAL_ERROR
          "In-source builds are not supported: please specify a build "
          "directory with -B or use a one of the available presets.")
endif()

# Set a common directory for "binaries"
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# A generator expression that is true for the build types that generate
# debugging information (for use as "$<${EDG_HAS_DEBUG_INFO}:...>").
set(EDG_HAS_DEBUG_INFO "$<CONFIG:Debug,RelWithDebInfo>")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if (UNIX AND NOT APPLE AND NOT EDG_ENABLE_GCC_BACKEND AND NOT EDG_ENABLE_LLVM_BACKEND)
    # Perform static linking of the binary.
    add_link_options("$<$<CONFIG:Release>:-static>")
  endif()
  # If we're building with GCC or any variant of Clang, use -g3 as our debug
  # flag to include macro information in builds with debugging information.
  add_compile_options("$<${EDG_HAS_DEBUG_INFO}:-g3>")
endif()

# Attempt to determine the best linker
if(NOT DEFINED EDG_PREFERRED_LINKER)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
      CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(NOT DEFINED EDG_PREFERRED_LINKER_TRIAL)
      find_program(MOLD_LINKER ld.mold NO_CACHE)
      if(NOT MOLD_LINKER STREQUAL "MOLD_LINKER-NOTFOUND")
        set(EDG_PREFERRED_LINKER_TRIAL mold)
      endif()
    endif()

    # Only use lld with Clang, it doesn't understand GCC's LTO format well (at
    # least at the time of writing).
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT APPLE)
      if(NOT DEFINED EDG_PREFERRED_LINKER_TRIAL)
        find_program(LLD_LINKER ld.lld NO_CACHE)
        if(NOT LLD_LINKER STREQUAL "LLD_LINKER-NOTFOUND")
          set(EDG_PREFERRED_LINKER_TRIAL lld)
        endif()
      endif()
    endif()

    # Avoid auto-selecting GNU ld.gold with Clang: gold is deprecated in
    # binutils and emits hundreds of "Cannot export local symbol '__asan_...'"
    # warnings on hidden assembly thunks listed in Clang's libclang_rt.asan
    # --dynamic-list when -fsanitize=address is active.
    if(NOT DEFINED EDG_PREFERRED_LINKER_TRIAL AND
       NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      find_program(GOLD_LINKER ld.gold NO_CACHE)
      if(NOT GOLD_LINKER STREQUAL "GOLD_LINKER-NOTFOUND")
        set(EDG_PREFERRED_LINKER_TRIAL gold)
      endif()
    endif()
  endif() # GCC OR CLANG

  if(DEFINED EDG_PREFERRED_LINKER_TRIAL)
    set(EDG_PREFERRED_LINKER "${EDG_PREFERRED_LINKER_TRIAL}" CACHE STRING
        "The preferred linker")
  endif()
endif() # EDG_PREFERRED_LINKER CACHE CHECK

if(DEFINED EDG_PREFERRED_LINKER)
  message(STATUS "Preferring linker: ${EDG_PREFERRED_LINKER}")
endif()

# Require EDG_BASE and attempt to build the library by default (unless using
# a direct LLVM or GCC code-generating backend where cpfe emits native IR/objects).
if(NOT WIN32 AND NOT EDG_ENABLE_LLVM_BACKEND AND NOT EDG_ENABLE_GCC_BACKEND)
  set(EDG_BUILD_LIB TRUE CACHE BOOL "Enable the EDG library build")
else()
  set(EDG_BUILD_LIB FALSE CACHE BOOL "Enable the EDG library build")
endif()

# Enable Testing
enable_testing()
include(CTest)

# For GCC and Clang provide an (off by default) option for colored output.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(FORCE_COLORED_OUTPUT FALSE CACHE BOOL "Enable colored output")
else()
  unset(FORCE_COLORED_OUTPUT CACHE)
endif()

set(EDG_EAGER_REBUILD_LIB FALSE CACHE BOOL
    "Make changes to the front end binary result in library source be rebuilt")

# Support for 32-bit build
set(32BIT_BUILD FALSE CACHE BOOL "Build cpfe as a 32-bit executable")

# Support for reproducible builds.  A build identifier replaces the build date
# and time in the front end (see src/CMakeLists.txt).
set(EDG_BUILD_IDENTIFIER "" CACHE STRING
    "Build identifier to use in place of the build date and time")
if(NOT "$CACHE{EDG_BUILD_IDENTIFIER}" STREQUAL "" AND
   (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  # In builds with debugging information, record the source and build
  # directories as relative paths in that information and in __FILE__, so the
  # same sources produce the same executables wherever they are checked out
  # and built.  The build directory is listed last so that it takes precedence
  # when it is inside the source directory.  (-ffile-prefix-map is avoided
  # because it also remaps the paths recorded for gcov, which coverage reports
  # use to find the sources.)
  add_compile_options(
      "$<${EDG_HAS_DEBUG_INFO}:-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.>"
      "$<${EDG_HAS_DEBUG_INFO}:-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.>"
      "$<${EDG_HAS_DEBUG_INFO}:-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build>"
      "$<${EDG_HAS_DEBUG_INFO}:-fmacro-prefix-map=${CMAKE_BINARY_DIR}=build>")
endif()

# Detect the defaults (macro configuration, EDG_BASE, etc.) for this host and
# compiler, used when a configuration doesn't specify them.
include(${CMAKE_SOURCE_DIR}/cmake/host-defaults.cmake)
detect_host_defaults()

# Include and initialize supporting logic for EDG cmake macro configurations.
include(${CMAKE_SOURCE_DIR}/cmake/macro-conf.cmake)
init_macro_configuration_support()

# Resolve EDG_BASE, preferring the environment over the host default.  The
# result is exported by environment.sh for use by eccp.
if(NOT "$ENV{EDG_BASE}" STREQUAL "")
  set(EDG_BASE "$ENV{EDG_BASE}" CACHE STRING "The EDG base to use.")
else()
  set(EDG_BASE "${EDG_HOST_BASE}" CACHE STRING "The EDG base to use.")
endif()

add_subdirectory(util)
add_subdirectory(src)

if($CACHE{EDG_BUILD_LIB})
  if(NOT WIN32)
    add_subdirectory(lib_src)
    # Check edg_eccp_config/EDG_BASE setup
    message(STATUS
            "Checking for edg_eccp_config in '$CACHE{EDG_BASE}'.")
    if("$CACHE{EDG_BASE}" STREQUAL "")
      message(FATAL_ERROR
              "There is no default EDG_BASE for this host and compiler; "
              "please set EDG_BASE to a folder with an 'edg_eccp_config'.")
    elseif(NOT EXISTS "$CACHE{EDG_BASE}/edg_eccp_config")
      message(FATAL_ERROR
              "Please set EDG_BASE to a folder with an 'edg_eccp_config'.")
    endif()
  else()
    # The runtime library build is not currently supported on Windows
    # due to the driver being a bash script.
    message(FATAL_ERROR
            "The EDG library build is not currently supported on Windows.")
  endif()
endif()

# Pull in support for wrapping scripts. Note this must be done before test
# integration setup, as test integration also depends on this functionality.
include(${CMAKE_SOURCE_DIR}/cmake/wrapper.cmake)

# Handle wrapping eccp, needed for building the C library, and general
# development
add_edg_wrapper_script(eccp "." ${CMAKE_SOURCE_DIR}/util/eccp.sh)

if(BUILD_TESTING AND EDG_ENABLE_LLVM_BACKEND)
  add_subdirectory(tests)
endif()

if(BUILD_TESTING AND EDG_ENABLE_GCC_BACKEND)
  add_subdirectory(tests/gcc_be)
endif()

if(EDG_ENABLE_GCC_BACKEND AND NOT WIN32 AND NOT TARGET gcc_be_eh_rt)
  enable_language(C CXX)
  add_library(gcc_be_eh_rt STATIC "${CMAKE_SOURCE_DIR}/lib_src/gcc_be_eh_rt.c")
  set_target_properties(gcc_be_eh_rt PROPERTIES
    OUTPUT_NAME "edg_gcc_eh_rt"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  target_include_directories(gcc_be_eh_rt PUBLIC
    "${CMAKE_SOURCE_DIR}/lib_src"
    "${CMAKE_SOURCE_DIR}/src"
  )
  target_compile_definitions(gcc_be_eh_rt PRIVATE BACK_END_IS_GCC_GEN_BE=1)
  target_compile_options(gcc_be_eh_rt PRIVATE
    "-x" "c++"
    "-std=c++17"
    "-fno-exceptions"
    "-fno-rtti"
    "-funwind-tables"
    "-fasynchronous-unwind-tables"
    "-Wunused-result"
    "-Werror=unused-result"
  )
endif()

if(EDG_ENABLE_GCC_BACKEND AND TARGET cpfe AND TARGET gcc_be_eh_rt)
  add_dependencies(cpfe gcc_be_eh_rt)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_SOURCE_DIR}/tests/driver/CMakeLists.txt")
  add_subdirectory(tests/driver)
endif()

if(EDG_ENABLE_LLVM_BACKEND)
  set(EDG_EXPECTED_LLVM_FLAG 1)
else()
  set(EDG_EXPECTED_LLVM_FLAG 0)
endif()
if(EDG_ENABLE_GCC_BACKEND)
  set(EDG_EXPECTED_GCC_FLAG 1)
else()
  set(EDG_EXPECTED_GCC_FLAG 0)
endif()
if(EDG_ENABLE_C_BACKEND)
  set(EDG_EXPECTED_C_FLAG 1)
else()
  set(EDG_EXPECTED_C_FLAG 0)
endif()
add_test(
  NAME verify_cmake_backend_mode
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/dev_tools/verify_backend_modes.py
          --cpfe $<TARGET_FILE:cpfe>
          --llvm ${EDG_EXPECTED_LLVM_FLAG}
          --gcc ${EDG_EXPECTED_GCC_FLAG}
          --c ${EDG_EXPECTED_C_FLAG}
)

# 100% Doc Coverage Check for LLVM Backend
if(EDG_ENABLE_LLVM_BACKEND)
  add_custom_target(
      check_doc_coverage ALL
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/dev_tools/check_doc_coverage.py
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Verifying 100% LLVM backend Doxygen coverage..."
  )
endif()

# 100% Doc Coverage Check for GCC Backend
if(EDG_ENABLE_GCC_BACKEND)
  add_custom_target(
      check_gcc_be_doc_coverage
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/dev_tools/check_gcc_be_doc_coverage.py
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Verifying 100% GCC backend Doxygen coverage..."
  )
endif()

# Packaging (CPack) Configuration for Multi-Platform Releases
set(CPACK_PACKAGE_NAME "edgcpp")
set(CPACK_PACKAGE_VENDOR "Edison Design Group")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "EDG C++ Front End with LLVM and GCC Backends")
set(CPACK_PACKAGE_VERSION_MAJOR "6")
set(CPACK_PACKAGE_VERSION_MINOR "6")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

if(WIN32)
  set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
  set(CPACK_GENERATOR "TGZ")
elseif(UNIX)
  set(CPACK_GENERATOR "TGZ")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "EDG")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgccjit0 (>= 10.0.0) | libgccjit-10-dev | libgccjit-11-dev | libgccjit-12-dev | libgccjit-13-dev | libgccjit-14-dev")
  set(CPACK_RPM_PACKAGE_REQUIRES "libgccjit")
endif()

include(CPack)
