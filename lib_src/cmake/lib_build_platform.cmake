# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Define and expand an EDG_CPP_RT_LIBS cache variable storing the list of
# enabled edg-cpp-rt libraries.
set(EDG_CPP_RT_LIBS "${EDG_HOST_CPP_RT_LIBS}" CACHE STRING
    "The EDG C++ runtime libs to include in the default target")
set(EXPANDED_CPP_RT_LIBS $CACHE{EDG_CPP_RT_LIBS})
separate_arguments(EXPANDED_CPP_RT_LIBS)
list(TRANSFORM EXPANDED_CPP_RT_LIBS PREPEND "edg-cpp-rt-")

function(add_edg_cpp_rt_for_target_impl platform)
  # Set a custom output folder for the library
  set(TARGET_LIB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib_${platform}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${TARGET_LIB_OUTPUT_DIR})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${TARGET_LIB_OUTPUT_DIR})

  add_edg_cpp_rt_target("edg-cpp-rt-${platform}" "--target=${platform}")

  # Since this is enabled, edg-cpp-rt-libs depend on this platform edg-cpp-rt
  add_dependencies(edg-cpp-rt-libs "edg-cpp-rt-${platform}")
endfunction()

function(add_edg_cpp_rt_for_target platform)
  if ("edg-cpp-rt-${platform}" IN_LIST EXPANDED_CPP_RT_LIBS)
    add_edg_cpp_rt_for_target_impl(${platform})
  endif()
endfunction()
