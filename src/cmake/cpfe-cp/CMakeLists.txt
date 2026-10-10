# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# List of all historic files for the front end.
set(ALL_C_FILES "${CORE_FRONT_END_SOURCE_FILES}")

# Load the specified macro configuration for cpfe.
macro_configuration_to_compile_definitions("cpfe-cp")

# compile .c files as C++.
set_source_files_properties(${ALL_C_FILES} PROPERTIES LANGUAGE CXX)

# Set the name of the primary binary (i.e., the one that actually contains
# the front end source code).
set(primary_front_end_target "cpfe-cp")

if($CACHE{MULTI_THREADED_DAEMON})
  # Add the daemon executable.
  add_executable(cpfe-cp-daemon ${ALL_C_FILES} ${TXT_FILES})
  add_dependencies(cpfe-cp-daemon error-info)

  # Additionally add the client executable.
  set(cli_src_file ${CMAKE_SOURCE_DIR}/util/cfe_daemon_client.c)
  set_source_files_properties(${cli_src_file} PROPERTIES LANGUAGE CXX)
  add_executable(cpfe-cp ${cli_src_file})

  # Allow the daemon client to see the project source directory in its include
  # path.
  target_include_directories(cpfe-cp PRIVATE ${PROJECT_SOURCE_DIR})

  # Update the primary binary name.
  set(primary_front_end_target "cpfe-cp-daemon")

  # Make the client depend on the daemon so the cpfe build target still
  # does the expected thing.
  add_dependencies(cpfe-cp cpfe-cp-daemon)
else()
  # Add the main cpfe-cp executable.
  add_executable(cpfe-cp ${ALL_C_FILES} ${TXT_FILES})
  add_dependencies(cpfe-cp error-info)
endif()

add_dependencies(cpfe-all cpfe-cp)

# Expose softfloat.
add_softfloat_to_target("${primary_front_end_target}")

if(ENABLE_LLVM_BACKEND)
  target_link_libraries(${primary_front_end_target} PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis)
endif()
