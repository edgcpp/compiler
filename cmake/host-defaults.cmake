# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Defaults derived from the host platform and compiler, so that a configure
# which doesn't use a preset (e.g., an IDE's default CMake profile) makes the
# same selections as the matching preset in CMakePresets.json.

function(detect_host_defaults)
  # This function sets up the variables:
  #  - EDG_HOST_MACRO_CONF: the debug macro configuration for this host and
  #    compiler (the release configuration, if any, adds a "-release"
  #    suffix), or "" if none is known.
  #  - EDG_HOST_BASE: the default EDG_BASE directory, or "" if none is known.
  #  - EDG_HOST_CPP_RT_LIBS: the default EDG_CPP_RT_LIBS list.
  set(macro_conf "")
  set(base "")
  set(cpp_rt_libs "")
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" processor)

  if(APPLE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      if(processor MATCHES "^(arm64|aarch64)$")
        set(macro_conf "macos-arm-clang")
        set(base "cmake-native/macos-arm/clang")
        set(cpp_rt_libs "macos_arm64")
      elseif(processor STREQUAL "x86_64")
        set(macro_conf "macos-x86-clang")
        set(base "cmake-native/macos-x86_64/clang")
        set(cpp_rt_libs "macos_x86_64")
      endif()
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # The Linux configurations all target an x86_64 (or, for 32-bit builds,
    # i686) host.
    if(processor MATCHES "^(x86_64|amd64)$")
      if($CACHE{32BIT_BUILD})
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
          set(macro_conf "linux-i686-gcc")
          set(base "docker/dev-env/gcc-i686")
          set(cpp_rt_libs "linux_i686 win32")
        endif()
      else()
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
          set(macro_conf "linux-gcc")
          set(base "docker/dev-env/gcc")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
          set(macro_conf "linux-clang")
          set(base "docker/dev-env/clang")
        endif()
        if(NOT macro_conf STREQUAL "")
          string(CONCAT cpp_rt_libs
                 "linux_riscv64 linux_riscv32 linux_aarch64 linux_armv7 "
                 "linux_i686 linux_x86_64 win32 win64")
        endif()
      endif()
    endif()
  elseif(WIN32 AND MSVC)
    if(processor MATCHES "^(x86_64|amd64)$")
      set(macro_conf "windows-msvc")
    endif()
  endif()

  if(NOT base STREQUAL "")
    set(base "${CMAKE_SOURCE_DIR}/bases/${base}")
  endif()

  # Export shared state.
  set(EDG_HOST_MACRO_CONF "${macro_conf}" PARENT_SCOPE)
  set(EDG_HOST_BASE "${base}" PARENT_SCOPE)
  set(EDG_HOST_CPP_RT_LIBS "${cpp_rt_libs}" PARENT_SCOPE)
endfunction()

function(host_macro_configuration result_var build_type)
  # Set result_var to the host's macro configuration for build_type, or to ""
  # if there isn't one.
  set(result "")
  if(NOT EDG_HOST_MACRO_CONF STREQUAL "")
    set(result "${EDG_HOST_MACRO_CONF}")
    string(TOUPPER "${build_type}" build_type)
    if(build_type MATCHES "^(RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
      string(APPEND result "-release")
    endif()
    if(NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/macro-conf/${result}")
      set(result "")
    endif()
  endif()
  set(${result_var} "${result}" PARENT_SCOPE)
endfunction()
