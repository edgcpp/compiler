# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.19)

# Set the project name
project(cpfe)

# Create a configurable variable for building the daemon.
set(MULTI_THREADED_DAEMON FALSE CACHE BOOL "Build the daemon/client")

# specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS NO)

# Optionally treat warnings as errors.
set(EDG_CPFE_WARNINGS_ARE_ERRORS FALSE CACHE BOOL "Treat warnings as errors")

# Enable additional warnings/features depending on the compiler.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
   CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # If we're building with GCC or any variant of Clang.
  add_compile_options(-Wall -Wwrite-strings -Wformat-security -pedantic
                      -Werror=shadow)

  # This option is only supported in newer GCC compilers.
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
     CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.1)
    add_compile_options(-Wimplicit-fallthrough)
  endif()

  # Optionally enable conversion warnings (these are still problematically
  # noisy in some configurations and with some older compilers).
  set(EDG_ENABLE_CONVERSION_WARNINGS TRUE CACHE BOOL
      "Enable -Wconversion and -Wsign-conversion")
  if($CACHE{EDG_ENABLE_CONVERSION_WARNINGS})
    add_compile_options(-Wconversion -Wsign-conversion)
  endif()

  # Enable release build related options and optimizations.
  #
  # Ensure in release builds all switches have a default case.
  add_compile_options("$<$<CONFIG:Release>:-Wswitch-default>")
  # For releases with debug information insure the frame pointer isn't
  # optimized away.
  add_compile_options("$<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>")
  # Enable link time optimization (on the compiler side).
  add_compile_options("$<$<CONFIG:Release>:-flto>")
  # Enable link time optimization (on the linker side).
  #
  # Note that -flto=auto is needed for best performance on GCC, but is not
  # recognized by Clang versions before 13.
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
     CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
    add_link_options("$<$<CONFIG:Release>:-flto=auto>")
  else()
    add_link_options("$<$<CONFIG:Release>:-flto>")
  endif()

  # Figure out the best default "use ASan" value.
  if(UNIX AND NOT APPLE)
    set(default_cpfe_use_asan TRUE)
  else()
    set(default_cpfe_use_asan FALSE)
  endif()
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(default_cpfe_use_asan FALSE)
  endif()
  # Optionally enable memory sanitization to make certain errors easier to
  # find.
  set(EDG_CPFE_USE_ASAN ${default_cpfe_use_asan} CACHE BOOL
      "Enable -fsanitize=address")
  if($CACHE{EDG_CPFE_USE_ASAN})
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
  endif()

  # Optionally treat warnings as errors.
  if($CACHE{EDG_CPFE_WARNINGS_ARE_ERRORS})
    add_compile_options(-Werror)
  endif()

  # If we found a better linker than the default, use it.
  if(DEFINED EDG_PREFERRED_LINKER)
    add_link_options("-fuse-ld=${EDG_PREFERRED_LINKER}")
  endif()
else()
  # Otherwise assume MSVC.

  # Enable support for big object files.
  add_compile_options(/bigobj)

  # Enable the majority of warnings.
  add_compile_options(/W4)

  # Disable some noisy warnings:
  # - Unused variable warning -- MSVC does not have a C++11 equivalent to
  #   [[maybe_unused]] so this becomes quite noisy.
  add_compile_options(/wd4100 /wd4189)
  # - Constant conditional expression -- MSVC warns when a condition is
  #   constant; this is used regularly in the code base (particularly for
  #   some cases where the preprocessor-based configuration differs) and
  #   is thus useless.
  add_compile_options(/wd4127)
  # - Signed & unsigned comparison - MSVC uses this warning when comparing
  #   bool and unsigned int bitfields (which are used regularly in the
  #   front end).
  add_compile_options(/wd4389)
  # - Unreachable code -- Similar to the above constant conditional
  #   expression warning, MSVC warns on unreachable code and this is not
  #   a useful warning for EDG purposes.
  add_compile_options(/wd4702)
  # - strcpy deprecation -- Windows is deprecating strcpy; we have a bug
  #   to resolve this, but it will take time.
  add_compile_options(/wd4996)

  # Optionally treat warnings as errors.
  if($CACHE{EDG_CPFE_WARNINGS_ARE_ERRORS})
    add_compile_options(/WX)
  endif()
endif()

# 32-bit build support

if($CACHE{32BIT_BUILD})
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
     CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-m32 -msse2)
    add_link_options(-m32 -msse2)
  else()
    message(FATAL_ERROR
            "32-bit builds are not (yet) supported for your compiler.")
  endif()
endif()

# gcov Support

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(EDG_CPFE_USE_GCOV FALSE CACHE BOOL "Enable gcov support")
  if($CACHE{EDG_CPFE_USE_GCOV})
    add_compile_options(-fprofile-arcs -ftest-coverage)
    add_link_options(-lgcov)
  endif()
endif()

# Enable colored output.

if (${FORCE_COLORED_OUTPUT})
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-fdiagnostics-color=always)
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options (-fcolor-diagnostics)
  endif ()
endif ()

# Disable undesirable C++ features.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # If we're building with GCC or any variant of Clang.
  add_compile_options(-fno-exceptions -fno-unwind-tables -fno-rtti)
else()
  # Otherwise assume MSVC.
  # FIXME: What are the options here?
endif()

# Ensure our source files are processed in C++ mode despite their file '.c'
# file extension.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-x c++)
endif()

# Create a directory for generated headers
set(PROJECT_HEADER_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/includes)
include_directories(${PROJECT_HEADER_OUTPUT_DIRECTORY})

# Process the specified macro configuration for cpfe into a cmake_defines.h.
macro_configuration_to_defines("base")

# Some generators (e.g. Unix Makefiles) require this folder to be generated
# explicitly.
file(MAKE_DIRECTORY ${PROJECT_HEADER_OUTPUT_DIRECTORY})

# TXT and ERR_H files are used by both cpfe and cdisp
set(TXT_FILES ${PROJECT_SOURCE_DIR}/error_msg.txt
              ${PROJECT_SOURCE_DIR}/error_tag.txt)
set(ERR_H_FILES ${PROJECT_HEADER_OUTPUT_DIRECTORY}/err_codes.h
                ${PROJECT_HEADER_OUTPUT_DIRECTORY}/err_data.h)

# Setup a target that uses mk_errinfo to generate err_codes.h and err_data.h
add_custom_command(
  OUTPUT ${ERR_H_FILES}
  COMMAND mk_errinfo ${TXT_FILES} ${ERR_H_FILES}
  COMMENT "Building err_codes.h and err_data.h"
  DEPENDS mk_errinfo ${TXT_FILES}
)
add_custom_target(error-info ALL DEPENDS ${ERR_H_FILES})

# Pull in support for softfloat.
include(${CMAKE_SOURCE_DIR}/cmake/softfloat.cmake)

# Define a common set of source files for cpfe and cpfe-cp.
set(CORE_FRONT_END_SOURCE_FILES
    attribute.c c_gen_be.c cfe.c class_decl.c cmd_line.c const_ints.c
    cp_gen_be.c debug.c decl_inits.c decl_spec.c declarator.c decls.c def_arg.c
    disambig.c error.c expr.c exprutil.c extasm.c fe_init.c fe_wrapup.c
    fixed_pt.c float_pt.c floating.c folding.c func_def.c host_envir.c
    ifc_modules.c il.c il_alloc.c il_display.c il_read.c il_to_str.c il_walk.c
    il_write.c inline.c interpret.c layout.c lexical.c literals.c lookup.c
    lower_c99.c lower_eh.c lower_il.c lower_init.c lower_name.c macro.c
    mem_manage.c modules.c ms_attrib.c overload.c pch.c pragma.c preproc.c
    scope_stk.c src_seq.c statements.c symbol_ref.c symbol_tbl.c sys_predef.c
    target.c templates.c trans_copy.c trans_corresp.c trans_unit.c types.c)

# Add newer additions here. This allows this CMake file to be used for older
# versions of the front end via edg-cmake-ctl.
macro(add_src_file_if_present file)
  if(EXISTS "${PROJECT_SOURCE_DIR}/${file}")
    list(APPEND CORE_FRONT_END_SOURCE_FILES ${file})
  endif()
endmacro()
add_src_file_if_present(cfe_daemon.c)
add_src_file_if_present(ifc_map_functions.c)
add_src_file_if_present(ifc_map_functions_acc.c)
add_src_file_if_present(ifc_map_functions_dbg.c)
add_src_file_if_present(ifc_map_functions_mut.c)
add_src_file_if_present(ifc_map_functions_val.c)
add_src_file_if_present(ifc_modules_read.c)
add_src_file_if_present(ifc_modules_templ.c)
add_src_file_if_present(ifc_modules_write.c)
add_src_file_if_present(unicode_name_fsm.c)

# Prepend the project source directory to the file names.
list(TRANSFORM CORE_FRONT_END_SOURCE_FILES PREPEND ${PROJECT_SOURCE_DIR}/)

# When compiling with Microsoft Visual Studio, include ms_metadata.cpp and
# also specify a directory where the atlcomcli.h header file can be found
# (it's not included in the Express version of Visual Studio so it might be in
# different places on different machines).
if(MSVC)
  list(APPEND CORE_FRONT_END_SOURCE_FILES
       "${PROJECT_SOURCE_DIR}/ms_metadata.cpp")
  link_libraries(mscoree)
endif()

if(EDG_ENABLE_GCC_BACKEND)
  add_compile_definitions(BACK_END_IS_GCC_GEN_BE=1)
  list(APPEND CORE_FRONT_END_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_error.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_context.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_cache.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_lib_loader.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_location.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_type.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_decl.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_expr.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_stmt.c"
    "${PROJECT_SOURCE_DIR}/gcc_gen_be_main.c"
  )
  link_libraries(LibGCCJIT::LibGCCJIT)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib;@rpath")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
  endif()
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  
  if(MSVC)
    add_link_options(/DELAYLOAD:libgccjit.dll)
  endif()
  
  if(BUILD_TESTING OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_source_files_properties("${PROJECT_SOURCE_DIR}/gcc_gen_be_error.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_context.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_cache.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_lib_loader.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_location.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_type.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_decl.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_expr.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_stmt.c" "${PROJECT_SOURCE_DIR}/gcc_gen_be_main.c" PROPERTIES COMPILE_OPTIONS "-fprofile-arcs;-ftest-coverage")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      add_link_options(-lgcov)
    else()
      add_link_options(--coverage)
    endif()
  endif()
endif()

add_custom_target(cpfe-all)

# A quirk of CMake is that scoping is really done in CMakeLists.txt files via
# the subdirectory system.  So, to allow cdisp, cpfe, and cpfe-cp to be built
# with different files and flags, we need to add the projects seperately.
add_subdirectory(cmake/cpfe)
add_subdirectory(cmake/cpfe-cp)
add_subdirectory(cmake/cdisp)


if(EDG_ENABLE_GCC_BACKEND)
  # Apply coverage instrumentation specifically to gcc_gen_be_*.c
  set_source_files_properties(gcc_gen_be_*.c PROPERTIES COMPILE_FLAGS "--coverage")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(gcc_gen_be_*.c PROPERTIES COMPILE_FLAGS "-fprofile-arcs -ftest-coverage")
    target_link_libraries(cpfe PRIVATE gcov)
  elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_source_files_properties(gcc_gen_be_*.c PROPERTIES COMPILE_FLAGS "--coverage")
    target_link_options(cpfe PRIVATE "--coverage")
  endif()
endif()
