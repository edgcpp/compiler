# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# FIXME: doesn't use lib_eccp (should it?)
cmake_minimum_required(VERSION 3.19)

# This is part of the hack to make CMake work with eccp. On MacOS we need to
# suppress the "-isysroot" flag that CMake adds default from the clang build.
if(UNIX AND APPLE)
  set(CMAKE_OSX_SYSROOT "")
endif()

# set the project name
project(lib_src NONE)

# Create deterministic archives (with zeroed timestamps, owners, and modes) on
# Linux, where ar and ranlib are expected to be GNU binutils or compatible,
# so that rebuilding the same sources produces the same libraries.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_CXX_ARCHIVE_CREATE
      "<CMAKE_AR> qcD <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> qD <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -D <TARGET>")
endif()

# Add the default value of EDG_RUNTIME_LIB to the cache.
if(NOT DEFINED CACHE{EDG_RUNTIME_LIB})
  # Use the same environment variables ECCP will be using.
  if(DEFINED ENV{EDG_RUNTIME_LIB})
    set(EDG_RUNTIME_LIB_TRAIL "$ENV{EDG_RUNTIME_LIB}")
  else()
    set(EDG_RUNTIME_LIB_TRAIL "C")
  endif()
  set(EDG_RUNTIME_LIB "${EDG_RUNTIME_LIB_TRAIL}" CACHE STRING
      "The primary name of the EDG runtime library")
endif()

# Add the default value of EDG_LIB_SUFFIX to the cache.
if(NOT DEFINED CACHE{EDG_LIB_SUFFIX})
  # Use the same environment variables ECCP will be using.
  if(DEFINED ENV{EDG_LIB_SUFFIX})
    set(EDG_LIB_SUFFIX_TRAIL "$ENV{EDG_LIB_SUFFIX}")
  else()
    set(EDG_LIB_SUFFIX_TRAIL "")
  endif()
  set(EDG_LIB_SUFFIX "${EDG_LIB_SUFFIX_TRAIL}" CACHE STRING
      "The suffix applied to the EDG runtime library")
endif()

set(BARE_FILE_NAMES array_del array_del_aligned array_new
    array_new_aligned array_nodel array_nodel_aligned array_nonew
    array_nonew_aligned array_pdel array_pnew bad_alloc
    del_virt delete delete_aligned delnothrow delnothrow_aligned
    dtor_list edg_exit eh_util error exception exit guard main
    memzero munch_ctors munch_dtors new new_aligned newhandler
    newnothrow newnothrow_aligned placedel placenew pure_virt rtti
    set_new sized_array_del sized_array_del_aligned sized_del
    sized_del_aligned static_init stdexcept thread_dtor throw
    typeinfo vars vec_cctor vec_newdel vla_alloc)

# Load the specified macro configuration for the runtime library (i.e.,
# cpprtlib).
macro_configuration_to_compile_definitions("cpprtlib")

# Add a custom target for all enabled C libraries
add_custom_target(edg-cpp-rt-libs)

# Pull in support functions for setting up the cmake/default and
# cmake/<platform> targets.
include(${PROJECT_SOURCE_DIR}/cmake/lib_build_support.cmake)

# A quirk of CMake is that scoping is really done in CMakeLists.txt files via
# the subdirectory system. So, to allow the different versions of libC to be
# built with different flags, we need to add the projects separately.
add_subdirectory(cmake/default)

# Pull in support for functions to build the different versions of libC.
include(${PROJECT_SOURCE_DIR}/cmake/lib_build_platform.cmake)

add_subdirectory(cmake/linux_aarch64)
add_subdirectory(cmake/linux_armv7)
add_subdirectory(cmake/linux_riscv64)
add_subdirectory(cmake/linux_riscv32)
add_subdirectory(cmake/linux_i686)
add_subdirectory(cmake/linux_x86_64)
add_subdirectory(cmake/macos_arm64)
add_subdirectory(cmake/macos_x86_64)
add_subdirectory(cmake/win32)
add_subdirectory(cmake/win64)
