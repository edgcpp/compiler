# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The following is code for generating supporting files for the CMake
# build.

string(JOIN "\n" static_env
    "export CPFE=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cpfe\""
    "export CPFE_CP=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cpfe-cp\""
    "export ECCP_LIBDIR=\"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}\""
    "export EDG_MUNCH_PATH=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/edg_munch\""
    "export EDG_DECODE_PATH=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/edg_decode\""
    "export EDG_PRELINK_PATH=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/edg_prelink\""
    "export ECCP=\"${CMAKE_SOURCE_DIR}/util/eccp.sh\"")

string(JOIN "\n" lazy_static_env
  "export CPFE=\"\${CPFE:-${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cpfe}\""
  "export CPFE_CP=\"\${CPFE_CP:-${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cpfe-cp}\""
  "export ECCP_LIBDIR=\"\${ECCP_LIBDIR:-${CMAKE_LIBRARY_OUTPUT_DIRECTORY}}\""
  "export EDG_MUNCH_PATH=\"\${EDG_MUNCH_PATH:-${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/edg_munch}\""
  "export EDG_DECODE_PATH=\"\${EDG_DECODE_PATH:-${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/edg_decode}\""
  "export EDG_PRELINK_PATH=\"\${EDG_PRELINK_PATH:-${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/edg_prelink}\""
  "export ECCP=\"\${ECCP:-${CMAKE_SOURCE_DIR}/util/eccp.sh}\"")

# Default EDG_BASE (without overriding an explicit setting) so that eccp works
# without it being set.
if(NOT EDG_RESOLVED_BASE STREQUAL "")
  set(base_env "export EDG_BASE=\"\${EDG_BASE:-${EDG_RESOLVED_BASE}}\"")
  string(APPEND static_env "\n${base_env}")
  string(APPEND lazy_static_env "\n${base_env}")
endif()

FILE(WRITE ${CMAKE_BINARY_DIR}/environment.sh "${static_env}\n")
FILE(WRITE ${CMAKE_BINARY_DIR}/.envrc
     "${static_env}\nsource_env \"${CMAKE_SOURCE_DIR}/.envrc\"\n")
FILE(WRITE ${CMAKE_BINARY_DIR}/lazy_environment.sh "${lazy_static_env}\n")

function(add_edg_wrapper_script script_name working_dir delegate_script_path)
  # Write a wrapper that sets environment variables for EDG scripts to
  # look at what we've built
  FILE(WRITE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${script_name}
    "#!/bin/sh\n"
    ". \"${CMAKE_BINARY_DIR}/environment.sh\"\n"
    "cd \"${working_dir}\"\n"
    "exec \"${delegate_script_path}\" \"$@\"\n")

  # File permission 755
  file(CHMOD ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${script_name}
       PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                   GROUP_READ GROUP_EXECUTE
                   WORLD_READ WORLD_EXECUTE)
endfunction()

