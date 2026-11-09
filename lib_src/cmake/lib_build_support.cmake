# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# This option has been removed, caching the result makings things unnecessarily
# painful when system paths include; if users need a static path, use the
# EDG_CINCLDIR environment variable (or perhaps we add another).
if(DEFINED CACHE{EDG_CINCLDIR})
  unset(EDG_CINCLDIR CACHE)
endif()

if(DEFINED ENV{EDG_CINCLDIR})
  message(STATUS "Using ENV variable EDG_CINCLDIR for C library paths.")
  set(EDG_DETECTED_CINCLDIR "$ENV{EDG_CINCLDIR}")
else()
  # Find the edg-scrape-compiler program.
  find_program(TOOL_PATH_EDG_SCRAPE_COMPILER
               NAMES edg-scrape-compiler
               PATHS "${CMAKE_SOURCE_DIR}/dev_tools/bin")

  if(UNIX AND NOT APPLE)
    message(STATUS "Using edg-scrape-compiler to detect GCC C library paths.")
    execute_process(COMMAND "${TOOL_PATH_EDG_SCRAPE_COMPILER}"
                            gcc --lang c includes
                    OUTPUT_VARIABLE EDG_DETECTED_CINCLDIR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
  elseif(UNIX)
    message(STATUS "Using edg-scrape-compiler to detect Clang C library "
                   "paths.")
    execute_process(COMMAND "${TOOL_PATH_EDG_SCRAPE_COMPILER}"
                            clang --lang c includes
                    OUTPUT_VARIABLE EDG_DETECTED_CINCLDIR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()

if(NOT DEFINED EDG_DETECTED_CINCLDIR)
  message(FATAL_ERROR "Please set EDG_CINCLDIR to the include path of the "
                      "C library the EDG runtime library should be built "
                      "against.")
endif()

message(STATUS "Using C library path: \"${EDG_DETECTED_CINCLDIR}\".")

string(REPLACE ":" ";" EDG_CINCLDIR_LIST "${EDG_DETECTED_CINCLDIR}")

# If one of the include paths changes (e.g., is deleted, automatically
# reconfigure). This detects things like GNU header paths changing on major GCC
# releases/distribution upgrades.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             ${EDG_CINCLDIR_LIST})

function(add_edg_cpp_rt_obj_file src_file_name obj_file_name)
  # Set up the dependency list for the object (by default depend on the
  # source file only, but if EDG_EAGER_REBUILD_LIBS is set, also depend on
  # changes to cpfe).
  set(obj_file_deps ${src_file_name})
  if($CACHE{EDG_EAGER_REBUILD_LIB})
    list(APPEND obj_file_deps ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cpfe)
  endif()

  # eccp runs in the object's directory (which is unique to the target
  # platform), where --keep_gen_c leaves the generated C file.  Otherwise eccp
  # would compile that file in a randomly named temporary directory, whose
  # name would end up in the debug information and make the library
  # irreproducible.
  get_filename_component(obj_dir "${obj_file_name}" DIRECTORY)

  # Generate debugging information only for the build types that have it.
  # For a reproducible build, also record the source and build directories as
  # relative paths in that information (as is done for the front end in the
  # top-level CMakeLists.txt).  Each option is wrapped separately so that it
  # disappears entirely in other build types.
  set(debug_options "$<${EDG_HAS_DEBUG_INFO}:-g>")
  if(NOT "$CACHE{EDG_BUILD_IDENTIFIER}" STREQUAL "")
    list(APPEND debug_options
      "$<${EDG_HAS_DEBUG_INFO}:--c_to_obj_option>"
      "$<${EDG_HAS_DEBUG_INFO}:-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.>"
      "$<${EDG_HAS_DEBUG_INFO}:--c_to_obj_option>"
      "$<${EDG_HAS_DEBUG_INFO}:-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build>")
  endif()

  add_custom_command(
    OUTPUT ${obj_file_name}
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/eccp
    # Set the base options.
    --building_runtime --c++23
    # Compile the generated C file in the working directory (see obj_dir
    # above).
    --keep_gen_c
    # Add options related to debug builds (as needed).
    ${debug_options}
    # Set the compile definitons.
    -DCHECKING=1 -DDEBUG=1
    ${cmakedef_define_flags}
    # Add include dirs.
    --no_standard_includes
    --sys_include=${CMAKE_SOURCE_DIR}/include_c++
    --sys_include=${CMAKE_SOURCE_DIR}/include_c99
    ${edg_cinl_flags}
    # Apply the emulation flag (if any).
    ${edg_target_emulation}
    # Enable position independent code so linking succeeds if this
    # file is archive is used on a different machine.
    --c_to_obj_option -fPIC
    # Forward any extra arguments passed to this function as flags
    ${ARGN}
    # Compile to the expected object.
    -o ${obj_file_name}
    -c ${src_file_name}
    DEPENDS ${obj_file_deps}
    WORKING_DIRECTORY ${obj_dir}
    # Drop the debug options whose generator expressions evaluate to nothing
    # (rather than passing eccp empty arguments).
    COMMAND_EXPAND_LISTS
  )
endfunction()

function(add_edg_cpp_rt_target target_name)
  set(edg_cinl_flags "")
  foreach(edg_cinl_dir IN LISTS EDG_CINCLDIR_LIST)
    # Skip empty entries (e.g., from a trailing ":" in EDG_CINCLDIR).
    if(NOT edg_cinl_dir STREQUAL "")
      get_filename_component(edg_cinl_dir "${edg_cinl_dir}" ABSOLUTE
                             BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
      list(APPEND edg_cinl_flags "--sys_include=${edg_cinl_dir}")
    endif()
  endforeach()

  if(UNIX AND NOT APPLE)
    # When building the library on Linux machines, we need to enable g++ mode
    # so that __asm function specifiers in the standard C library headers are
    # understood.
    set(edg_target_emulation "--g++")
  elseif(UNIX)
    # When building the library on MacOS machines, we need to enable clang
    # mode so NULL redefinitions specified in the standard C library headers
    # do not break the build.
    set(edg_target_emulation "--clang")
  else()
    set(edg_target_emulation "")
  endif()

  # Convert the compile definitons (from the cmakedef files) into flags.
  get_directory_property(cmakedef_define_flags COMPILE_DEFINITIONS)
  list(TRANSFORM cmakedef_define_flags PREPEND "-D")

  # Create an empty variable to be used as a list of object files.
  set(obj_files "")

  # Explicitly create the cache directory (needed for some generators -- e.g.,
  # Unix Makefiles).
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/${target_name}-cache")

  # Add the build for the main object files.
  foreach(bare_file_name IN LISTS BARE_FILE_NAMES)
    set(src_file_name "${PROJECT_SOURCE_DIR}/${bare_file_name}.c")
    set(obj_file_name
        "${PROJECT_BINARY_DIR}/${target_name}-cache/${bare_file_name}.o")
    list(APPEND obj_files ${obj_file_name})
    add_edg_cpp_rt_obj_file(${src_file_name} ${obj_file_name} ${ARGN})
  endforeach()

  # Add the build for c99_complex.c which needs a warning disabled.
  set(c99_complex_file_name "${PROJECT_SOURCE_DIR}/c99_complex.c")
  set(c99_complex_obj_file_name
      "${PROJECT_BINARY_DIR}/${target_name}-cache/c99_complex.o")
  list(APPEND obj_files ${c99_complex_obj_file_name})
  add_edg_cpp_rt_obj_file(${c99_complex_file_name}
                          ${c99_complex_obj_file_name}
                          # Disable the lossy conversion warning (this is
                          # expected and left on for customers).
                          --diag_suppress lossy_conversion
                          # Forward ARGN
                          ${ARGN})

  # Add the build for decode.c a special file pulled from util used to get
  # __cxa_demangle for IA-64 ABI.
  set(decode_file_name "${CMAKE_SOURCE_DIR}/util/decode.c")
  set(decode_obj_file_name
      "${PROJECT_BINARY_DIR}/${target_name}-cache/decode.o")
  list(APPEND obj_files ${decode_obj_file_name})
  add_edg_cpp_rt_obj_file(${decode_file_name} ${decode_obj_file_name}
                          # Add special flags for decode
                          -DCOMPILE_DECODE_FOR_LIB_SRC=1
                          -DDEFAULT_EMULATE_GNU_ABI_BUGS=1
                          -DUSE_LONG_DOUBLE_FOR_HOST_FP_VALUE=0
                          # Add source include directory
                          -I${CMAKE_SOURCE_DIR}/src
                          # Forward ARGN
                          ${ARGN})

  # Declare the library
  add_library(${target_name} ${obj_files})
  set_target_properties(${target_name} PROPERTIES LINKER_LANGUAGE CXX)

  # Normalize the name based on the configuration/expectations of ECCP.
  # Additionally, set whether this is enabled by default.
  set_target_properties(${target_name} PROPERTIES
                        ARCHIVE_OUTPUT_NAME
                        "$CACHE{EDG_RUNTIME_LIB}$CACHE{EDG_LIB_SUFFIX}"
                        LIBRARY_OUTPUT_NAME
                        "$CACHE{EDG_RUNTIME_LIB}$CACHE{EDG_LIB_SUFFIX}")

  # Our runtime library depends on the cpfe build.
  add_dependencies(${target_name} cpfe)
endfunction()

