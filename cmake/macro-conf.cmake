# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Find the edg-cmakedef program.
if(WIN32)
  find_program(TOOL_PATH_EDG_CMAKEDEF
               NAMES edg-cmakedef.bat
               PATHS "${CMAKE_SOURCE_DIR}/dev_tools/win-bin")
else()
  find_program(TOOL_PATH_EDG_CMAKEDEF
               NAMES edg-cmakedef
               PATHS "${CMAKE_SOURCE_DIR}/dev_tools/bin")
endif()

function(resolve_macro_configuration result_var macro_conf build_type)
  # Set result_var to macro_conf, or, if that's empty, to the host's macro
  # configuration for build_type.
  set(result "${macro_conf}")
  if(result STREQUAL "")
    host_macro_configuration(result "${build_type}")
    if(result STREQUAL "")
      message(WARNING
              "No macro configuration matches this host, compiler, and "
              "build type \"${build_type}\"; set EDG_MACRO_CONF or use a "
              "preset from CMakePresets.json.")
    else()
      message(STATUS "Auto-selected macro configuration \"${result}\" for "
                     "build type \"${build_type}\".")
    endif()
  endif()
  set(${result_var} "${result}" PARENT_SCOPE)
endfunction()

function(init_macro_configuration_support)
  # An empty macro configuration selects the one matching the host platform,
  # compiler, and build type.  The selections are exported as
  # CACHE{EDG_MACRO_CONF} (single-configuration generators) or
  # CACHE{EDG_DEBUG_MACRO_CONF} and CACHE{EDG_RELEASE_MACRO_CONF}
  # (multi-configuration generators).
  if(CMAKE_CONFIGURATION_TYPES)
    resolve_macro_configuration(debug_conf "$CACHE{EDG_DEBUG_MACRO_CONF}"
                                "Debug")
    resolve_macro_configuration(release_conf
                                "$CACHE{EDG_RELEASE_MACRO_CONF}" "Release")
    set(EDG_DEBUG_MACRO_CONF "${debug_conf}" CACHE STRING
        "The macro configuration for debug builds")
    set(EDG_RELEASE_MACRO_CONF "${release_conf}" CACHE STRING
        "The macro configuration for release builds")
  else()
    resolve_macro_configuration(conf "${EDG_MACRO_CONF}"
                                "${CMAKE_BUILD_TYPE}")
    set(EDG_MACRO_CONF "${conf}" CACHE STRING
        "The macro configuration for the build")
  endif()
endfunction()

function(process_macro_configuration_setup target macro_conf)
  # This function sets up the variables:
  #  - macro_conf_root_path
  #  - macro_conf_path
  #
  # It additionally handles some common setup of the macro configuration
  # defaults.

  set(macro_conf_root_path "${CMAKE_SOURCE_DIR}/cmake/macro-conf")
  set(macro_conf_set_path "${macro_conf_root_path}/${macro_conf}")
  set(macro_conf_path "${macro_conf_set_path}/${target}.cmakedef")
  message(STATUS "Loading macro configuration:")
  message(STATUS "  ${TOOL_PATH_EDG_CMAKEDEF} \"${macro_conf_root_path}\" "
                 "\"${macro_conf_path}\"")

  if(NOT EXISTS "${macro_conf_path}")
    message(STATUS "Configuration missing, generating default file.")
    FILE(WRITE ${macro_conf_path}
               "# Auto Generated Default File\n"
               "import <default/${target}>\n")
  endif()

  # Add file dependencies.
  execute_process(COMMAND "${TOOL_PATH_EDG_CMAKEDEF}"
                            "${macro_conf_root_path}"
                            "${macro_conf_path}"
                            print-paths
                  OUTPUT_VARIABLE watched_files
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  RESULTS_VARIABLE cmd_status_code)
  if(NOT cmd_status_code EQUAL "0")
    message(FATAL_ERROR "Failed to process cmakedef.")
  endif()
  string(REPLACE "\n" ";" watched_files "${watched_files}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
               ${watched_files})

  # Export shared state.
  set(macro_conf_root_path ${macro_conf_root_path} PARENT_SCOPE)
  set(macro_conf_path ${macro_conf_path} PARENT_SCOPE)
endfunction()

function(configure_definitions_as_values target macro_conf)
  process_macro_configuration_setup("${target}" "${macro_conf}")
  # Add the compile definitions.
  execute_process(COMMAND "${TOOL_PATH_EDG_CMAKEDEF}"
                            "${macro_conf_root_path}"
                            "${macro_conf_path}"
                            print-values
                  OUTPUT_VARIABLE definitions_from_cmakedef
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ECHO_ERROR_VARIABLE
                  RESULTS_VARIABLE cmd_status_code)
  if(NOT cmd_status_code EQUAL "0")
    message(FATAL_ERROR "Failed to process cmakedef.")
  endif()
  string(REPLACE "\n" ";" definitions_from_cmakedef
         "${definitions_from_cmakedef}")
  # This will trigger any macros in definitions_from_cmake to be expanded
  # into their respective values (i.e., "@FOO@" -> the value of FOO in
  # CMake).
  if(EDG_ENABLE_LLVM_BACKEND OR ENABLE_LLVM_BACKEND)
    set(EDG_LLVM_GEN_BE_DEFAULT 1)
  else()
    set(EDG_LLVM_GEN_BE_DEFAULT 0)
  endif()
  if(EDG_ENABLE_GCC_BACKEND)
    set(EDG_GCC_GEN_BE_DEFAULT 1)
  else()
    set(EDG_GCC_GEN_BE_DEFAULT 0)
  endif()
  if(EDG_ENABLE_C_BACKEND)
    set(EDG_C_GEN_BE_DEFAULT 1)
  else()
    set(EDG_C_GEN_BE_DEFAULT 0)
  endif()
  if(EDG_LLVM_GEN_BE_DEFAULT OR EDG_GCC_GEN_BE_DEFAULT)
    set(EDG_DO_FULL_PORTABLE_EH_LOWERING_DEFAULT 0)
    set(EDG_GENERATE_EH_TABLES_DEFAULT 0)
  elseif(EDG_ENABLE_C_BACKEND)
    set(EDG_DO_FULL_PORTABLE_EH_LOWERING_DEFAULT 1)
    set(EDG_GENERATE_EH_TABLES_DEFAULT 1)
  else()
    set(EDG_DO_FULL_PORTABLE_EH_LOWERING_DEFAULT 0)
    set(EDG_GENERATE_EH_TABLES_DEFAULT 0)
  endif()
  string(CONFIGURE "${definitions_from_cmakedef}"
         configured_cmakedef_definitions)
  # Export shared state.
  set(configured_cmakedef_definitions ${configured_cmakedef_definitions}
      PARENT_SCOPE)
endfunction()

function(configure_definitions_as_defines target macro_conf)
  process_macro_configuration_setup("${target}" "${macro_conf}")
  # Add the compile definitions.
  execute_process(COMMAND "${TOOL_PATH_EDG_CMAKEDEF}"
                            "${macro_conf_root_path}"
                            "${macro_conf_path}"
                            print-defines
                  OUTPUT_VARIABLE definitions_from_cmakedef
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ECHO_ERROR_VARIABLE
                  RESULTS_VARIABLE cmd_status_code)
  if(NOT cmd_status_code EQUAL "0")
    message(FATAL_ERROR "Failed to process cmakedef.")
  endif()
  # This will trigger any macros in definitions_from_cmake to be expanded into
  # their respective values (i.e., "@FOO@" -> the value of FOO in CMake).
  if(EDG_ENABLE_LLVM_BACKEND OR ENABLE_LLVM_BACKEND)
    set(EDG_LLVM_GEN_BE_DEFAULT 1)
  else()
    set(EDG_LLVM_GEN_BE_DEFAULT 0)
  endif()
  if(EDG_ENABLE_GCC_BACKEND)
    set(EDG_GCC_GEN_BE_DEFAULT 1)
  else()
    set(EDG_GCC_GEN_BE_DEFAULT 0)
  endif()
  if(EDG_ENABLE_C_BACKEND)
    set(EDG_C_GEN_BE_DEFAULT 1)
  else()
    set(EDG_C_GEN_BE_DEFAULT 0)
  endif()
  if(EDG_LLVM_GEN_BE_DEFAULT OR EDG_GCC_GEN_BE_DEFAULT)
    set(EDG_DO_FULL_PORTABLE_EH_LOWERING_DEFAULT 0)
    set(EDG_GENERATE_EH_TABLES_DEFAULT 0)
  elseif(EDG_ENABLE_C_BACKEND)
    set(EDG_DO_FULL_PORTABLE_EH_LOWERING_DEFAULT 1)
    set(EDG_GENERATE_EH_TABLES_DEFAULT 1)
  else()
    set(EDG_DO_FULL_PORTABLE_EH_LOWERING_DEFAULT 0)
    set(EDG_GENERATE_EH_TABLES_DEFAULT 0)
  endif()
  string(CONFIGURE "${definitions_from_cmakedef}"
         configured_cmakedef_definitions)
  # Export shared state with an extra trailing newline to avoid warnings from
  # self-hosted builds.
  set(configured_cmakedef_definitions "${configured_cmakedef_definitions}\n"
      PARENT_SCOPE)
endfunction()

function(macro_configuration_to_compile_definitions target)
  if(CMAKE_CONFIGURATION_TYPES)
    # Capture the definitions for a debug configuration.
    configure_definitions_as_values("${target}"
                                     "$CACHE{EDG_DEBUG_MACRO_CONF}")
    set(debug_args "${configured_cmakedef_definitions}")
    # Capture the definitions for a release configuration.
    configure_definitions_as_values("${target}"
                                     "$CACHE{EDG_RELEASE_MACRO_CONF}")
    set(release_args "${configured_cmakedef_definitions}")
    # Conditionally add either the debug or release configuration defines.
    add_compile_definitions(
                         "$<IF:$<CONFIG:Debug>,${debug_args},${release_args}>")
  else()
    configure_definitions_as_values("${target}" "$CACHE{EDG_MACRO_CONF}")
    add_compile_definitions(${configured_cmakedef_definitions})
  endif()
endfunction()

function(macro_configuration_to_defines target)
  set(cmake_defines_file_path
      "${PROJECT_HEADER_OUTPUT_DIRECTORY}/cmake_defines.h")
  if(CMAKE_CONFIGURATION_TYPES)
    # Write out a defines.h for the debug configuration.
    configure_definitions_as_defines("${target}"
                                     "$CACHE{EDG_DEBUG_MACRO_CONF}")
    FILE(WRITE "${PROJECT_HEADER_OUTPUT_DIRECTORY}/cmake_debug_defines.h"
               "${configured_cmakedef_definitions}")
    # Write out a defines.h for the release configuration.
    configure_definitions_as_defines("${target}"
                                     "$CACHE{EDG_RELEASE_MACRO_CONF}")
    FILE(WRITE "${PROJECT_HEADER_OUTPUT_DIRECTORY}/cmake_release_defines.h"
               "${configured_cmakedef_definitions}")
    # Generate a defines.h file that will include either the debug or
    # release versions of the defines.h file generated above.
    string(CONCAT cmake_defines_contents
                  "#if CMAKE_DEBUG\n"
                  "#include \"cmake_debug_defines.h\"\n"
                  "#else\n"
                  "#include \"cmake_release_defines.h\"\n"
                  "#endif")
    FILE(WRITE "${cmake_defines_file_path}" "${cmake_defines_contents}")
    # Add a compile definition to select the appropriate defines.h file.
    add_compile_definitions(
                           "$<IF:$<CONFIG:Debug>,CMAKE_DEBUG=1,CMAKE_DEBUG=0>")
  else()
    configure_definitions_as_defines("${target}" "$CACHE{EDG_MACRO_CONF}")
    FILE(WRITE "${cmake_defines_file_path}"
               "${configured_cmakedef_definitions}")
  endif()
  add_compile_definitions("USE_CMAKE_DEFINES")
endfunction()

