# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# FIXME: cfront/IA64 ABI?
cmake_minimum_required(VERSION 3.19)

project(util)

# If we found a better linker than the default, use it.
if(DEFINED EDG_PREFERRED_LINKER AND NOT (APPLE AND EDG_PREFERRED_LINKER STREQUAL "lld"))
  add_link_options("-fuse-ld=${EDG_PREFERRED_LINKER}")
endif()

# 32-bit build support

if($CACHE{32BIT_BUILD})
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
     CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-m32)
    add_link_options(-m32)
  else()
    message(FATAL_ERROR
            "32-bit builds are not (yet) supported for your compiler.")
  endif()
endif()

add_executable(edg_munch edg_munch.c)
add_executable(edg_prelink edg_prelink.c decode.c)
add_executable(edg_decode edg_decode.c decode.c)
add_executable(mk_errinfo mk_errinfo.c)
add_executable(dettarg dettarg.c)

# Treat .c files as C++ code:
set(C_FILES decode.c dettarg.c edg_decode.c edg_munch.c edg_prelink.c
            mk_errinfo.c)
set_source_files_properties(${C_FILES} PROPERTIES LANGUAGE CXX)

# Use C++11 for all source files:
set_property(TARGET edg_munch PROPERTY CXX_STANDARD 14)
set_property(TARGET edg_prelink PROPERTY CXX_STANDARD 14)
set_property(TARGET edg_decode PROPERTY CXX_STANDARD 14)
set_property(TARGET mk_errinfo PROPERTY CXX_STANDARD 14)
set_property(TARGET dettarg PROPERTY CXX_STANDARD 14)

# Add -I options for all executables:
include_directories(${PROJECT_SOURCE_DIR}/../src)

# Add compiler-specific options (suppressing warnings with dettarg):
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Ensure our source files are processed in C++ mode despite their file '.c'
  # file extension.
  set(shared_options -x c++)
  set(options ${shared_options} -Wall -Wextra -Wshadow
              -Wunused -Wwrite-strings)
  set(dettarg_options ${shared_options} -w)
else()
  # Otherwise assume MSVC.
  set(options /W2)
  set(dettarg_options /w)
endif()

target_compile_options(edg_munch PRIVATE ${options})
target_compile_options(edg_prelink PRIVATE ${options})
target_compile_options(edg_decode PRIVATE ${options})
target_compile_options(mk_errinfo PRIVATE ${options})
target_compile_options(dettarg PRIVATE ${dettarg_options})
