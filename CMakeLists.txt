# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.19)

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND APPLE)
  # Release builds on MacOS are built for both the ARM and x86_64
  # architectures.
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

# set the project name
project(cpfe)

# Setup CMake module path to find our custom modules (like FindLibGCCJIT.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# GCC Backend Option
option(EDG_ENABLE_GCC_BACKEND "Enable the libgccjit backend" OFF)
if(EDG_ENABLE_GCC_BACKEND)
  find_package(LibGCCJIT REQUIRED)
endif()

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

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if (UNIX AND NOT APPLE)
    # Perform static linking of the binary.
    add_link_options("$<$<CONFIG:Release>:-static>")
  endif()
  # If we're building with GCC or any variant of Clang, use -g3 as our debug
  # flag to include macro information in our debug builds.
  add_compile_options("$<$<CONFIG:Debug>:-g3>")
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
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      if(NOT DEFINED EDG_PREFERRED_LINKER_TRIAL)
        find_program(LLD_LINKER ld.lld NO_CACHE)
        if(NOT LLD_LINKER STREQUAL "LLD_LINKER-NOTFOUND")
          set(EDG_PREFERRED_LINKER_TRIAL lld)
        endif()
      endif()
    endif()

    if(NOT DEFINED EDG_PREFERRED_LINKER_TRIAL)
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

# Require EDG_BASE and attempt to build the library by default.
if(NOT WIN32)
  set(EDG_BUILD_LIB TRUE CACHE BOOL "Enable the EDG library build")
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
            "Checking for edg_eccp_config in \"$CACHE{EDG_BASE}\".")
    if("$CACHE{EDG_BASE}" STREQUAL "")
      message(FATAL_ERROR
              "There is no default EDG_BASE for this host and compiler; "
              "please set EDG_BASE to a folder with an \"edg_eccp_config\".")
    elseif(NOT EXISTS "$CACHE{EDG_BASE}/edg_eccp_config")
      message(FATAL_ERROR
              "Please set EDG_BASE to a folder with an \"edg_eccp_config\".")
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

if(EDG_ENABLE_GCC_BACKEND)
  add_subdirectory(tests/gcc_be)
endif()

# --- CPack Integration ---
set(CPACK_PACKAGE_NAME "edg-cpfe")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "EDG C++ Front End (with GCC backend)")
set(CPACK_PACKAGE_VENDOR "Edison Design Group")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

if(APPLE)
  set(CPACK_GENERATOR "DragNDrop;TGZ")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP;WIX")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(CPACK_GENERATOR "TXZ")
else()
  set(CPACK_GENERATOR "DEB;RPM")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "EDG")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgccjit0 (>= 10.0.0) | libgccjit-10-dev | libgccjit-11-dev | libgccjit-12-dev | libgccjit-13-dev | libgccjit-14-dev")
  set(CPACK_RPM_PACKAGE_REQUIRES "libgccjit")
endif()

include(CPack)
