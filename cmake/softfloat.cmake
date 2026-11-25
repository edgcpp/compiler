# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

function(add_softfloat_to_target target_name)
  if(DEFINED ENV{EDG_SOFTFLOAT_INCLUDE_PATH})
    message(STATUS "Adding softfloat library to ${target_name} include path.")
    target_include_directories("${target_name}" PRIVATE
                               "$ENV{EDG_SOFTFLOAT_INCLUDE_PATH}")
  endif()
  if(DEFINED ENV{EDG_SOFTFLOAT_LIB_PATH})
    message(STATUS "Adding softfloat library to ${target_name} link line.")
    target_link_libraries("${target_name}" PRIVATE
                          "$ENV{EDG_SOFTFLOAT_LIB_PATH}")
  elseif(DEFINED ENV{EDG_SOFTFLOAT_LIB_BASE_PATH})
    message(STATUS "Adding softfloat library to ${target_name} link line.")
    if($CACHE{32BIT_BUILD})
      target_link_libraries(
                           "${target_name}" PRIVATE
                           "$ENV{EDG_SOFTFLOAT_LIB_BASE_PATH}/x86/softfloat.a")
    else()
      target_link_libraries(
                        "${target_name}" PRIVATE
                        "$ENV{EDG_SOFTFLOAT_LIB_BASE_PATH}/x86_64/softfloat.a")
    endif()
  endif()
endfunction()

