# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.19)

project(edg_tools)

# Specify the C++ standard.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS NO)

# Default install too the dev_tools directory (where this CMakeLists.txt file
# is).
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${CMAKE_SOURCE_DIR}")
endif()

if(WIN32)
  set(CMAKE_INSTALL_BINDIR "win-bin")
endif()

# Add the various tool projects.
add_subdirectory(cpp_tools/check_closing_comments)
add_subdirectory(cpp_tools/check_defines)
add_subdirectory(cpp_tools/get_comments)
add_subdirectory(cpp_tools/process_unicode_names)
add_subdirectory(cpp_tools/remove_escapes)
