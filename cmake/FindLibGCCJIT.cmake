# FindLibGCCJIT
# -------------

file(GLOB _homebrew_libgccjit_include_dirs
  "/opt/homebrew/opt/libgccjit/include"
  "/usr/local/opt/libgccjit/include"
  "/opt/homebrew/Cellar/libgccjit/*/include"
  "/usr/local/Cellar/libgccjit/*/include"
  "/usr/lib/gcc/*/*/include"
)

file(GLOB _homebrew_libgccjit_lib_dirs
  "/opt/homebrew/opt/libgccjit/lib"
  "/opt/homebrew/opt/libgccjit/lib/gcc/current"
  "/opt/homebrew/opt/libgccjit/lib/gcc/*"
  "/usr/local/opt/libgccjit/lib"
  "/usr/local/opt/libgccjit/lib/gcc/current"
  "/usr/local/opt/libgccjit/lib/gcc/*"
  "/opt/homebrew/Cellar/libgccjit/*/lib/gcc/current"
  "/usr/local/Cellar/libgccjit/*/lib/gcc/current"
  "/usr/lib/gcc/*/*"
)

set(_msys2_libgccjit_include_dirs)
if(NOT MSVC)
  list(APPEND _msys2_libgccjit_include_dirs
    "C:/msys64/mingw64/include"
    "C:/msys64/ucrt64/include"
    "C:/msys64/clang64/include"
    "C:/msys64/clangarm64/include"
  )
endif()

find_path(LibGCCJIT_INCLUDE_DIR
  NAMES libgccjit.h
  HINTS
    ENV EDG_LIBGCCJIT_INCLUDE_DIR
  PATHS 
    /usr/include 
    /usr/local/include 
    /opt/homebrew/include 
    /opt/local/include
    ${_homebrew_libgccjit_include_dirs}
    "${CMAKE_SOURCE_DIR}/win-gccjit-include"
    "${CMAKE_SOURCE_DIR}/dev_tools/builtins"
    ${_msys2_libgccjit_include_dirs}
    "C:/Program Files/GCC/include"
)

find_library(LibGCCJIT_LIBRARY
  NAMES gccjit libgccjit.dll.a
  HINTS
    ENV EDG_LIBGCCJIT_LIB_DIR
  PATHS 
    /usr/lib 
    /usr/local/lib 
    /usr/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
    /opt/homebrew/lib
    /opt/homebrew/lib/gcc/current
    /opt/local/lib
    ${_homebrew_libgccjit_lib_dirs}
    "C:/msys64/mingw64/lib"
    "C:/msys64/ucrt64/lib"
    "C:/msys64/clangarm64/lib"
    "C:/Program Files/GCC/lib"
)

include(FindPackageHandleStandardArgs)
if(WIN32)
  find_package_handle_standard_args(LibGCCJIT
    REQUIRED_VARS LibGCCJIT_INCLUDE_DIR
  )
else()
  find_package_handle_standard_args(LibGCCJIT
    REQUIRED_VARS LibGCCJIT_LIBRARY LibGCCJIT_INCLUDE_DIR
  )
endif()

if(LibGCCJIT_FOUND)
  set(LibGCCJIT_INCLUDE_DIRS ${LibGCCJIT_INCLUDE_DIR})
  set(LibGCCJIT_LIBRARIES ${LibGCCJIT_LIBRARY})
  
  if(NOT TARGET LibGCCJIT::LibGCCJIT)
    if(WIN32 AND (MSVC OR NOT LibGCCJIT_LIBRARY))
      add_library(LibGCCJIT::LibGCCJIT INTERFACE IMPORTED)
      set_target_properties(LibGCCJIT::LibGCCJIT PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LibGCCJIT_INCLUDE_DIR}"
      )
    else()
      add_library(LibGCCJIT::LibGCCJIT UNKNOWN IMPORTED)
      set_target_properties(LibGCCJIT::LibGCCJIT PROPERTIES
        IMPORTED_LOCATION "${LibGCCJIT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibGCCJIT_INCLUDE_DIR}"
      )
    endif()
  endif()

  include(CheckCSourceCompiles)
  set(CMAKE_REQUIRED_INCLUDES ${LibGCCJIT_INCLUDE_DIR})
  
  # Inspect header libgccjit.h for feature availability macros
  macro(check_libgccjit_feature FEATURE)
    check_c_source_compiles("
      #if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
      #include <stddef.h>
      typedef ptrdiff_t ssize_t;
      #define _SSIZE_T_DEFINED
      #endif
      #include <libgccjit.h>
      #ifndef ${FEATURE}
      #error \"Feature ${FEATURE} not available\"
      #endif
      int main() { return 0; }
    " ${FEATURE})
  endmacro()

  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_context_new_bitcast)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_context_new_rvalue_from_vector)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_version)
  check_libgccjit_feature(LIBGCCJIT_HAVE_ASM_GOTO)
  check_libgccjit_feature(LIBGCCJIT_HAVE_ATTRIBUTES)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_lvalue_set_tls_model)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_block_add_try_catch)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_block_end_with_invoke)
  check_libgccjit_feature(LIBGCCJIT_HAVE_gcc_jit_block_add_landing_pad)
endif()

mark_as_advanced(LibGCCJIT_INCLUDE_DIR LibGCCJIT_LIBRARY)
