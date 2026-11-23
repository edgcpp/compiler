# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.20)

include(${CMAKE_SOURCE_DIR}/cmake/softfloat.cmake)

option(EDG_DRIVER_ENABLE_COVERAGE "Build Stage-0 shared driver tests with Clang source coverage" OFF)

add_executable(test_edg_be_driver
  test_edg_be_driver.cpp
  ${CMAKE_SOURCE_DIR}/src/edg_be_sdk.cpp
  ${CMAKE_SOURCE_DIR}/src/edg_be_driver.cpp
)

add_softfloat_to_target(test_edg_be_driver)

target_include_directories(test_edg_be_driver PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src/includes
)

target_compile_definitions(test_edg_be_driver PRIVATE USE_CMAKE_DEFINES)

if(TARGET error-info)
  add_dependencies(test_edg_be_driver error-info)
endif()

if(MSVC)
  target_compile_options(test_edg_be_driver PRIVATE
    /std:c++17 /EHs-c- /GR- /W4 /WX
    /wd4100 /wd4189 /wd4127 /wd4389 /wd4702 /wd4996 /bigobj
  )
  target_compile_definitions(test_edg_be_driver PRIVATE
    _HAS_EXCEPTIONS=0
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
  )
  target_link_options(test_edg_be_driver PRIVATE "/STACK:67108864")
else()
  target_compile_options(test_edg_be_driver PRIVATE
    -std=c++17
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Wunused-result
    -Werror=unused-result
  )
  if(EDG_DRIVER_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(test_edg_be_driver PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(test_edg_be_driver PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  endif()
endif()

add_test(NAME test_edg_be_driver COMMAND test_edg_be_driver)
