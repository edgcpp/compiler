# FindLibGCCJIT
# -------------

find_path(LibGCCJIT_INCLUDE_DIR
  NAMES libgccjit.h
  PATHS 
    /usr/include 
    /usr/local/include 
    /opt/homebrew/include 
    /opt/local/include
    "C:/msys64/mingw64/include"
    "C:/Program Files/GCC/include"
)

find_library(LibGCCJIT_LIBRARY
  NAMES gccjit
  PATHS 
    /usr/lib 
    /usr/local/lib 
    /usr/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
    /opt/homebrew/lib
    /opt/homebrew/lib/gcc/current
    /opt/local/lib
    "C:/msys64/mingw64/lib"
    "C:/Program Files/GCC/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibGCCJIT
  REQUIRED_VARS LibGCCJIT_LIBRARY LibGCCJIT_INCLUDE_DIR
)

if(LibGCCJIT_FOUND)
  set(LibGCCJIT_INCLUDE_DIRS ${LibGCCJIT_INCLUDE_DIR})
  set(LibGCCJIT_LIBRARIES ${LibGCCJIT_LIBRARY})
  
  if(NOT TARGET LibGCCJIT::LibGCCJIT)
    add_library(LibGCCJIT::LibGCCJIT UNKNOWN IMPORTED)
    set_target_properties(LibGCCJIT::LibGCCJIT PROPERTIES
      IMPORTED_LOCATION "${LibGCCJIT_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${LibGCCJIT_INCLUDE_DIR}"
    )
  endif()

  include(CheckCSourceCompiles)
  set(CMAKE_REQUIRED_INCLUDES ${LibGCCJIT_INCLUDE_DIR})
  
  # Inspect header libgccjit.h for feature availability macros
  macro(check_libgccjit_feature FEATURE)
    check_c_source_compiles("
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
endif()

mark_as_advanced(LibGCCJIT_INCLUDE_DIR LibGCCJIT_LIBRARY)
