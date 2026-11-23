# CMakeLists.txt for gcc_be tests

option(EDG_GCC_BE_ENABLE_COVERAGE "Enable code coverage flags for GCC backend" OFF)
add_compile_options(-Wunused-result -Werror=unused-result)
if(EDG_GCC_BE_ENABLE_COVERAGE)
    file(GLOB _GCC_BE_CPFE_SRCS "${PROJECT_SOURCE_DIR}/src/gcc_gen_be_*.c")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-fprofile-update=atomic" _GCC_BE_HAS_ATOMIC_PROF)
        set(_GCC_BE_CLANG_COV_FLAGS -O0 -g -fprofile-instr-generate -fcoverage-mapping -fno-inline)
        if(_GCC_BE_HAS_ATOMIC_PROF)
            list(APPEND _GCC_BE_CLANG_COV_FLAGS -fprofile-update=atomic)
        endif()
        add_compile_options(${_GCC_BE_CLANG_COV_FLAGS})
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
        if(TARGET cpfe AND _GCC_BE_CPFE_SRCS)
            set_property(SOURCE ${_GCC_BE_CPFE_SRCS}
                TARGET_DIRECTORY cpfe
                APPEND PROPERTY COMPILE_OPTIONS
                ${_GCC_BE_CLANG_COV_FLAGS} "-Wunused-result" "-Werror=unused-result")
            target_link_options(cpfe PRIVATE "-fprofile-instr-generate" "-fcoverage-mapping")
        endif()
    else()
        add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-inline)
        add_link_options(--coverage)
        if(TARGET cpfe AND _GCC_BE_CPFE_SRCS)
            set_property(SOURCE ${_GCC_BE_CPFE_SRCS}
                TARGET_DIRECTORY cpfe
                APPEND PROPERTY COMPILE_OPTIONS
                "-O0" "-g" "--coverage" "-fprofile-arcs" "-ftest-coverage" "-fno-inline" "-Wunused-result" "-Werror=unused-result")
            target_link_options(cpfe PRIVATE "--coverage")
        endif()
    endif()
endif()

# Real system STL headers (<iostream>, <vector>, <thread>, <coroutine>, <source_location>) discovery
if(APPLE)
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE _HOST_MAC_SDK OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir OUTPUT_VARIABLE _HOST_CLANG_RES OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(_HOST_PREDEF_HDR "${CMAKE_CURRENT_BINARY_DIR}/host_stl_predef.h")
    execute_process(
        COMMAND bash -c "${CMAKE_CXX_COMPILER} -dM -E -x c++ -std=c++20 /dev/null | grep -E '^#define __(BYTE_ORDER|ORDER_|CHAR_BIT|LP64|arm64|aarch64|x86_64|APPLE|MACH|LITTLE_ENDIAN|SIZEOF_|PTRDIFF_|SIZE_|WCHAR_|WINT_|INT|UINT|ATOMIC_|GCC_|GNUC|FINITE_MATH|GXX_|FLT_|DBL_|LDBL_|DECIMAL_DIG|BIGGEST_ALIGNMENT|USER_LABEL_PREFIX)' | python3 -c 'import sys; [print(\"#ifndef \" + l.split()[1].split(\"(\")[0] + \"\\n\" + l.strip() + \"\\n#endif\") for l in sys.stdin]' > '${_HOST_PREDEF_HDR}'"
    )
    set(_STL_CPFE_FLAGS
        "--target=macos_arm64"
        "--instantiate=used"
        "--clang"
        "--clang_version=190100"
        "--preinclude=${_HOST_PREDEF_HDR}"
        "--sys_include=${_HOST_MAC_SDK}/usr/include/c++/v1"
        "--sys_include=${_HOST_CLANG_RES}/include"
        "--sys_include=${_HOST_MAC_SDK}/usr/include"
    )
else()
    set(_STL_CPFE_FLAGS "--instantiate=used" "--g++" "--gnu_version=130200")
endif()

if(NOT TARGET gcc_be_eh_rt)
    add_library(gcc_be_eh_rt STATIC ${PROJECT_SOURCE_DIR}/lib_src/gcc_be_eh_rt.c)
    set_target_properties(gcc_be_eh_rt PROPERTIES
        OUTPUT_NAME "edg_gcc_eh_rt"
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
    )
    target_include_directories(gcc_be_eh_rt PRIVATE ${PROJECT_SOURCE_DIR}/lib_src ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(gcc_be_eh_rt PRIVATE BACK_END_IS_GCC_GEN_BE=1)
    target_compile_options(gcc_be_eh_rt PRIVATE
        "-x" "c++"
        "-std=c++17"
        "-fno-exceptions"
        "-fno-rtti"
        "-funwind-tables"
        "-fasynchronous-unwind-tables"
        "-Wunused-result"
        "-Werror=unused-result"
    )
endif()

set(GCC_BE_TEST_FILES
    test_scalars.c
    test_types.c
    test_expr_ops.c
    test_short_circuit.c
    test_control_flow.c
    test_functions.c
    test_aggregates.c
    test_cpp_classes.cpp
    test_virtual.cpp
    test_eh.cpp
    test_inline_asm.c
    test_edge_cases.c
    test_dynamic_init.cpp
    test_noexcept_raii.cpp
    test_types_numerics_exec.cpp
)

add_custom_target(run_gcc_be_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running GCC Backend tests"
)

set(GCC_BE_EXEC_LINK_FLAGS "")
if(EDG_GCC_BE_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(GCC_BE_EXEC_LINK_FLAGS "-fprofile-instr-generate" "-fcoverage-mapping")
    else()
        set(GCC_BE_EXEC_LINK_FLAGS "--coverage")
    endif()
endif()

foreach(test_file ${GCC_BE_TEST_FILES})
    set(EXTRA_FLAGS "")
    if(test_file STREQUAL "test_virtual.cpp")
        set(EXTRA_FLAGS "--c++11" "--no_exceptions" "--sys_include=${PROJECT_SOURCE_DIR}/include_c++")
    elseif(test_file STREQUAL "test_eh.cpp")
        set(EXTRA_FLAGS "--c++11" "--exceptions" "--sys_include=${PROJECT_SOURCE_DIR}/include_c++")
    elseif(test_file STREQUAL "test_types_numerics_exec.cpp")
        set(EXTRA_FLAGS "--c++20" "--no_exceptions" ${_STL_CPFE_FLAGS})
    elseif(test_file STREQUAL "test_noexcept_raii.cpp")
        set(EXTRA_FLAGS "--c++14" "--no_exceptions" "--sys_include=${PROJECT_SOURCE_DIR}/include_c++")
    elseif(test_file MATCHES "\.cpp$")
        set(EXTRA_FLAGS "--c++" "--no_exceptions")
    elseif(test_file STREQUAL "test_aggregates.c")
        set(EXTRA_FLAGS "--c" "--c99")
    elseif(test_file STREQUAL "test_edge_cases.c" OR test_file STREQUAL "test_control_flow.c")
        set(EXTRA_FLAGS "--c" "--gcc")
    elseif(test_file MATCHES "\.c$")
        set(EXTRA_FLAGS "--c" "--c99")
    endif()

    add_test(NAME test_gcc_be_${test_file}
             COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be ${EXTRA_FLAGS} -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o ${CMAKE_CURRENT_SOURCE_DIR}/${test_file})
    set_tests_properties(test_gcc_be_${test_file} PROPERTIES
        FIXTURES_SETUP "gcc_be_test_run"
        ENVIRONMENT "EDG_BASE=${PROJECT_SOURCE_DIR}/bases/cmake-native/macos-arm/clang")

    if(test_file MATCHES "\.cpp$")
        add_test(NAME test_gcc_be_link_${test_file}
                 COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 ${GCC_BE_EXEC_LINK_FLAGS} ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o $<TARGET_FILE:gcc_be_eh_rt> -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.exe)
    else()
        add_test(NAME test_gcc_be_link_${test_file}
                 COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.exe)
    endif()
    set_tests_properties(test_gcc_be_link_${test_file} PROPERTIES DEPENDS test_gcc_be_${test_file})

    add_test(NAME test_gcc_be_exec_${test_file}
             COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.exe)
    set_tests_properties(test_gcc_be_exec_${test_file} PROPERTIES DEPENDS test_gcc_be_link_${test_file})
endforeach()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(EDG_GCC_BE_ENABLE_COVERAGE)
    add_test(NAME test_gcc_be_coverage COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check_coverage.py)
    set_tests_properties(test_gcc_be_coverage PROPERTIES FIXTURES_REQUIRED "gcc_be_test_run")
endif()

add_test(NAME test_gcc_be_doc_coverage COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/dev_tools/check_gcc_be_doc_coverage.py)

add_custom_target(coverage_gcc_be
    COMMAND ${CMAKE_CTEST_COMMAND} -R test_gcc_be_coverage --output-on-failure
)

# Unified compile definitions and flags matching cpfe 1-for-1 so llvm-cov merges cpfe + unit test coverage seamlessly
set(_GCC_BE_UNIT_DEFS
    BACK_END_IS_C_GEN_BE=1
    BACK_END_IS_GCC_GEN_BE=1
    BACK_END_IS_LLVM_GEN_BE=0
    DO_FULL_PORTABLE_EH_LOWERING=0
    DO_IL_LOWERING=1
    GENERATE_EH_TABLES=0
    USE_CMAKE_DEFINES
)
set(_GCC_BE_UNIT_CXX_FLAGS "-x" "c++" "-std=c++17" "-fno-exceptions" "-fno-unwind-tables" "-fno-rtti")

# Unit tests for backend components
add_executable(test_error test_error.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_error error-info)
target_include_directories(test_error PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_options(test_error PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_compile_definitions(test_error PRIVATE ${_GCC_BE_UNIT_DEFS})
add_test(NAME test_error_unit COMMAND test_error)

add_executable(test_gcc_be_errors test_gcc_be_errors.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_gcc_be_errors error-info)
target_include_directories(test_gcc_be_errors PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_options(test_gcc_be_errors PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_compile_definitions(test_gcc_be_errors PRIVATE ${_GCC_BE_UNIT_DEFS})
add_test(NAME test_gcc_be_errors_unit COMMAND test_gcc_be_errors)

add_executable(test_gcc_be_context test_gcc_be_context.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_context error-info)
target_include_directories(test_gcc_be_context PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_context PRIVATE LibGCCJIT::LibGCCJIT)
target_compile_options(test_gcc_be_context PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_compile_definitions(test_gcc_be_context PRIVATE ${_GCC_BE_UNIT_DEFS})
add_test(NAME test_gcc_be_context_unit COMMAND test_gcc_be_context)

add_executable(test_gcc_be_lib_loader test_gcc_be_lib_loader.cpp)
add_dependencies(test_gcc_be_lib_loader error-info)
target_include_directories(test_gcc_be_lib_loader PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_lib_loader PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_lib_loader PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
add_test(NAME test_gcc_be_lib_loader_unit COMMAND test_gcc_be_lib_loader)

add_executable(test_gcc_be_cache test_gcc_be_cache.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c)
add_dependencies(test_gcc_be_cache error-info)
target_include_directories(test_gcc_be_cache PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_cache PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_cache PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
add_test(NAME test_gcc_be_cache_unit COMMAND test_gcc_be_cache)

add_executable(test_gcc_be_type test_gcc_be_type.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_type error-info)
target_include_directories(test_gcc_be_type PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_type PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_type PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_type PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_type_unit COMMAND test_gcc_be_type)

add_executable(test_gcc_be_rtti test_gcc_be_rtti.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_rtti error-info)
target_include_directories(test_gcc_be_rtti PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_rtti PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_rtti PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_rtti PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_rtti_unit COMMAND test_gcc_be_rtti)

add_executable(test_gcc_be_builtin test_gcc_be_builtin.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_builtin.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_atomic.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_simd.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_builtin error-info)
target_include_directories(test_gcc_be_builtin PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_builtin PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_builtin PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_builtin PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_builtin_unit COMMAND test_gcc_be_builtin)

add_executable(test_gcc_be_decl test_gcc_be_decl.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_decl.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c)
add_dependencies(test_gcc_be_decl error-info)
target_include_directories(test_gcc_be_decl PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_decl PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_decl PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_decl PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_decl_unit COMMAND test_gcc_be_decl)

add_executable(test_gcc_be_expr test_gcc_be_expr.cpp test_gcc_be_expr_lval.cpp test_gcc_be_expr_rval.cpp test_gcc_be_expr_rval2.cpp test_gcc_be_expr_cov_init_const.cpp test_gcc_be_expr_cov_lval.cpp test_gcc_be_expr_cov_rval.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_expr error-info)
target_include_directories(test_gcc_be_expr PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_expr PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_expr PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_expr PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_expr_unit COMMAND test_gcc_be_expr)

add_executable(test_gcc_be_abi test_gcc_be_abi.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_abi.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_abi error-info)
target_include_directories(test_gcc_be_abi PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_abi PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_abi PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_abi PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_abi_unit COMMAND test_gcc_be_abi)

add_executable(test_gcc_be_debug test_gcc_be_debug.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_debug.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_debug error-info)
target_include_directories(test_gcc_be_debug PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_debug PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_debug PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_debug PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_debug_unit COMMAND test_gcc_be_debug)

add_executable(test_gcc_be_dtor test_gcc_be_dtor.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_dtor.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_dtor error-info)
target_include_directories(test_gcc_be_dtor PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_dtor PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_dtor PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_dtor PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_dtor_unit COMMAND test_gcc_be_dtor)

add_executable(test_gcc_be_stmt2 test_gcc_be_stmt2.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_coro.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_omp.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_atomic.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_stmt2 error-info)
target_include_directories(test_gcc_be_stmt2 PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_stmt2 PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_stmt2 PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_stmt2 PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_stmt2_unit COMMAND test_gcc_be_stmt2)

add_executable(test_gcc_be_stmt test_gcc_be_stmt.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_coro.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_omp.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_atomic.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_stmt error-info)
target_include_directories(test_gcc_be_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_stmt PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_stmt PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_stmt PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_stmt_unit COMMAND test_gcc_be_stmt)

add_executable(test_gcc_be_error test_gcc_be_error.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_gcc_be_error error-info)
target_include_directories(test_gcc_be_error PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_error PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_error PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
add_test(NAME test_gcc_be_error_unit COMMAND test_gcc_be_error)

add_executable(test_gcc_be_loader test_gcc_be_loader.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_loader error-info)
target_include_directories(test_gcc_be_loader PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_loader PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_loader PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_loader PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_loader_unit COMMAND test_gcc_be_loader)

add_executable(test_gcc_be_coroutines test_gcc_be_coroutines.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_coro.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_omp.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_atomic.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_coroutines error-info)
target_include_directories(test_gcc_be_coroutines PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_coroutines PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_coroutines PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_coroutines PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_coroutines_unit COMMAND test_gcc_be_coroutines)

add_executable(test_gcc_be_abi_targets test_gcc_be_abi_targets.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_abi.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_abi_targets error-info)
target_include_directories(test_gcc_be_abi_targets PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_abi_targets PRIVATE ${_GCC_BE_UNIT_DEFS} "EDG_GCC_BE_TEST_SCRATCH_DIR=\"${PROJECT_SOURCE_DIR}/.tmp/agent-gcc\"")
target_compile_options(test_gcc_be_abi_targets PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_abi_targets PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_abi_targets_unit COMMAND test_gcc_be_abi_targets)

add_executable(test_gcc_be_bitfields test_gcc_be_bitfields.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_atomic.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_bitfields error-info)
target_include_directories(test_gcc_be_bitfields PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_bitfields PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_bitfields PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_bitfields PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_bitfields_unit COMMAND test_gcc_be_bitfields)

add_executable(test_gcc_be_rtti_full test_gcc_be_rtti_full.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_rtti_full error-info)
target_include_directories(test_gcc_be_rtti_full PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_rtti_full PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_rtti_full PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_rtti_full PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_rtti_full_unit COMMAND test_gcc_be_rtti_full)

add_executable(test_gcc_be_eh test_gcc_be_eh.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_eh.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_eh error-info)
target_include_directories(test_gcc_be_eh PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_eh PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_eh PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_eh PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_eh_unit COMMAND test_gcc_be_eh)

add_executable(test_gcc_be_eh_dwarf test_gcc_be_eh_dwarf.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_eh.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_eh_dwarf error-info)
target_include_directories(test_gcc_be_eh_dwarf PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_eh_dwarf PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_eh_dwarf PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_eh_dwarf PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_eh_dwarf_unit COMMAND test_gcc_be_eh_dwarf)

add_executable(test_gcc_be_simd test_gcc_be_simd.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_builtin.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_atomic.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_simd.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_simd error-info)
target_include_directories(test_gcc_be_simd PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_simd PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_simd PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_simd PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_simd_unit COMMAND test_gcc_be_simd)

add_executable(test_gcc_be_debug_dwarf test_gcc_be_debug_dwarf.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_debug.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_debug_dwarf error-info)
target_include_directories(test_gcc_be_debug_dwarf PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_debug_dwarf PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_debug_dwarf PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_debug_dwarf PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_debug_dwarf_unit COMMAND test_gcc_be_debug_dwarf)

add_executable(test_gcc_be_main test_gcc_be_main.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_main.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_main error-info)
target_include_directories(test_gcc_be_main PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_main PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_main PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_main PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_main_unit COMMAND test_gcc_be_main)

add_executable(test_gcc_be_driver test_gcc_be_driver.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_main.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_driver error-info)
target_include_directories(test_gcc_be_driver PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_driver PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_driver PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_driver PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_driver_unit COMMAND test_gcc_be_driver)

add_executable(test_gcc_be_location test_gcc_be_location.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_location error-info)
target_include_directories(test_gcc_be_location PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_location PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_location PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_location PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_location_unit COMMAND test_gcc_be_location)

add_executable(test_gcc_be_regression test_gcc_be_regression.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_gcc_be_regression error-info)
target_include_directories(test_gcc_be_regression PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_regression PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_regression PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
add_test(NAME test_gcc_be_regression_unit COMMAND test_gcc_be_regression)

add_executable(test_gcc_be_eh_rt test_gcc_be_eh_rt_unit.cpp)
target_include_directories(test_gcc_be_eh_rt PRIVATE ${PROJECT_SOURCE_DIR}/lib_src ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(test_gcc_be_eh_rt PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_eh_rt PRIVATE "-x" "c++" "-std=c++17" "-fno-exceptions" "-fno-rtti" "-funwind-tables" "-fasynchronous-unwind-tables" ${GCC_BE_COV_FLAGS})
target_link_libraries(test_gcc_be_eh_rt PRIVATE gcc_be_eh_rt)
add_test(NAME test_gcc_be_eh_rt_unit COMMAND test_gcc_be_eh_rt)

add_executable(test_gcc_be_omp_atomic_coro
    test_gcc_be_omp_atomic_coro_unit.cpp
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_atomic.c
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_simd.c
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_coro.c
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_omp.c
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c
    ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c
)
add_dependencies(test_gcc_be_omp_atomic_coro error-info)
target_include_directories(test_gcc_be_omp_atomic_coro PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_omp_atomic_coro PRIVATE ${_GCC_BE_UNIT_DEFS})
target_compile_options(test_gcc_be_omp_atomic_coro PRIVATE ${_GCC_BE_UNIT_CXX_FLAGS})
target_link_libraries(test_gcc_be_omp_atomic_coro PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_omp_atomic_coro_unit COMMAND test_gcc_be_omp_atomic_coro)

# Section 7.1: -O2 Incomplete external array `extern int arr[];` accessed at `arr[200]`
add_test(NAME test_gcc_be_incomp_arr_def_o2
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c --c99 -O2 -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_incomp_arr_def.o ${CMAKE_CURRENT_SOURCE_DIR}/test_incomp_arr_def.c)
add_test(NAME test_gcc_be_incomp_arr_use_o2
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c --c99 -O2 -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_incomp_arr_use.o ${CMAKE_CURRENT_SOURCE_DIR}/test_incomp_arr_use.c)
add_test(NAME test_gcc_be_link_incomp_arr_o2
    COMMAND ${CMAKE_C_COMPILER} -O2 ${CMAKE_CURRENT_BINARY_DIR}/test_incomp_arr_def.o ${CMAKE_CURRENT_BINARY_DIR}/test_incomp_arr_use.o -o ${CMAKE_CURRENT_BINARY_DIR}/test_incomp_arr_o2.exe)
set_tests_properties(test_gcc_be_link_incomp_arr_o2 PROPERTIES DEPENDS "test_gcc_be_incomp_arr_def_o2;test_gcc_be_incomp_arr_use_o2")
add_test(NAME test_gcc_be_exec_incomp_arr_o2 COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_incomp_arr_o2.exe)
set_tests_properties(test_gcc_be_exec_incomp_arr_o2 PROPERTIES DEPENDS test_gcc_be_link_incomp_arr_o2)

# Resolve libgomp and libatomic link flags for native C11/C23 atomic + OpenMP execution tests
set(_GCC_BE_LIBATOMIC_LINK "-latomic")
set(_GCC_BE_LIBGOMP_LINK "-lgomp")
if(APPLE)
    if(EXISTS "/opt/homebrew/opt/gcc/lib/gcc/current/libgomp.dylib")
        set(_GCC_BE_LIBGOMP_LINK "-L/opt/homebrew/opt/gcc/lib/gcc/current" "-Wl,-rpath,/opt/homebrew/opt/gcc/lib/gcc/current" "-lgomp")
        set(_GCC_BE_LIBATOMIC_LINK "-L/opt/homebrew/opt/gcc/lib/gcc/current" "-Wl,-rpath,/opt/homebrew/opt/gcc/lib/gcc/current" "-latomic")
    elseif(EXISTS "/usr/local/opt/gcc/lib/gcc/current/libgomp.dylib")
        set(_GCC_BE_LIBGOMP_LINK "-L/usr/local/opt/gcc/lib/gcc/current" "-Wl,-rpath,/usr/local/opt/gcc/lib/gcc/current" "-lgomp")
        set(_GCC_BE_LIBATOMIC_LINK "-L/usr/local/opt/gcc/lib/gcc/current" "-Wl,-rpath,/usr/local/opt/gcc/lib/gcc/current" "-latomic")
    endif()
endif()

# Section 7.1: C23 _BitInt(64/128), C11 _Atomic(T), libatomic struct CAS, C23 stdbit, and exact 2D VLA
add_test(NAME test_gcc_be_c23_atomics_vla
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c23 --clang --clang_version=230000 --gnu_version=140200 -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_c23_atomics_vla.o ${CMAKE_CURRENT_SOURCE_DIR}/test_c23_atomics_vla.c)
add_test(NAME test_gcc_be_link_c23_atomics_vla
    COMMAND ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/test_c23_atomics_vla.o ${_GCC_BE_LIBATOMIC_LINK} -o ${CMAKE_CURRENT_BINARY_DIR}/test_c23_atomics_vla.exe)
set_tests_properties(test_gcc_be_link_c23_atomics_vla PROPERTIES DEPENDS test_gcc_be_c23_atomics_vla)
add_test(NAME test_gcc_be_exec_c23_atomics_vla COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_c23_atomics_vla.exe)
set_tests_properties(test_gcc_be_exec_c23_atomics_vla PROPERTIES DEPENDS test_gcc_be_link_c23_atomics_vla)

# Section 7.1: OpenMP `#pragma omp parallel for reduction(+:sum)` linked with GNU `-lgomp`
add_test(NAME test_gcc_be_omp_exec
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c --c99 -fopenmp -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_omp_exec.o ${CMAKE_CURRENT_SOURCE_DIR}/test_omp_exec.c)
add_test(NAME test_gcc_be_link_omp_exec
    COMMAND ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/test_omp_exec.o ${_GCC_BE_LIBGOMP_LINK} -o ${CMAKE_CURRENT_BINARY_DIR}/test_omp_exec.exe)
set_tests_properties(test_gcc_be_link_omp_exec PROPERTIES DEPENDS test_gcc_be_omp_exec)
add_test(NAME test_gcc_be_exec_omp_exec COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_omp_exec.exe)
set_tests_properties(test_gcc_be_exec_omp_exec PROPERTIES DEPENDS test_gcc_be_link_omp_exec)

# Multi-TU C++ (<iostream> global initializers in 2+ .cpp files, weak _ZTV/_ZTI, namespaces, out-of-line methods)
add_test(NAME test_gcc_be_multi_tu_a
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c++17 --no_exceptions ${_STL_CPFE_FLAGS} -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_multi_tu_a.o ${CMAKE_CURRENT_SOURCE_DIR}/test_multi_tu_a.cpp)
add_test(NAME test_gcc_be_multi_tu_b
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c++17 --no_exceptions ${_STL_CPFE_FLAGS} -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_multi_tu_b.o ${CMAKE_CURRENT_SOURCE_DIR}/test_multi_tu_b.cpp)
add_test(NAME test_gcc_be_link_multi_tu
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 ${GCC_BE_EXEC_LINK_FLAGS} ${CMAKE_CURRENT_BINARY_DIR}/test_multi_tu_a.o ${CMAKE_CURRENT_BINARY_DIR}/test_multi_tu_b.o $<TARGET_FILE:gcc_be_eh_rt> -o ${CMAKE_CURRENT_BINARY_DIR}/test_multi_tu.exe)
set_tests_properties(test_gcc_be_link_multi_tu PROPERTIES DEPENDS "test_gcc_be_multi_tu_a;test_gcc_be_multi_tu_b")
add_test(NAME test_gcc_be_exec_multi_tu COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_multi_tu.exe)
set_tests_properties(test_gcc_be_exec_multi_tu PROPERTIES DEPENDS test_gcc_be_link_multi_tu)

# Native STL EH interop (std::vector::at(5), cross-TU RAII unwind, 16x1000 std::thread stress, MI & diamond catch)
add_test(NAME test_gcc_be_stl_eh_a
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c++17 --exceptions ${_STL_CPFE_FLAGS} -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_stl_eh_a.o ${CMAKE_CURRENT_SOURCE_DIR}/test_stl_eh_a.cpp)
add_test(NAME test_gcc_be_stl_eh_b
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c++17 --exceptions ${_STL_CPFE_FLAGS} -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_stl_eh_b.o ${CMAKE_CURRENT_SOURCE_DIR}/test_stl_eh_b.cpp)
add_test(NAME test_gcc_be_link_stl_eh
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 -pthread ${GCC_BE_EXEC_LINK_FLAGS} ${CMAKE_CURRENT_BINARY_DIR}/test_stl_eh_a.o ${CMAKE_CURRENT_BINARY_DIR}/test_stl_eh_b.o $<TARGET_FILE:gcc_be_eh_rt> -o ${CMAKE_CURRENT_BINARY_DIR}/test_stl_eh.exe)
set_tests_properties(test_gcc_be_link_stl_eh PROPERTIES DEPENDS "test_gcc_be_stl_eh_a;test_gcc_be_stl_eh_b")
add_test(NAME test_gcc_be_exec_stl_eh COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_stl_eh.exe)
set_tests_properties(test_gcc_be_exec_stl_eh PROPERTIES DEPENDS test_gcc_be_link_stl_eh)

# C++20 Coroutines with large 4,096-byte local array surviving across co_await (redzone + ASan verification)
add_test(NAME test_gcc_be_coro_large_frame
    COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be --c++20 --no_exceptions ${_STL_CPFE_FLAGS} -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/test_coro_large_frame.o ${CMAKE_CURRENT_SOURCE_DIR}/test_coro_large_frame.cpp)
add_test(NAME test_gcc_be_link_coro_large_frame
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 ${GCC_BE_EXEC_LINK_FLAGS} ${CMAKE_CURRENT_BINARY_DIR}/test_coro_large_frame.o $<TARGET_FILE:gcc_be_eh_rt> -o ${CMAKE_CURRENT_BINARY_DIR}/test_coro_large_frame.exe)
set_tests_properties(test_gcc_be_link_coro_large_frame PROPERTIES DEPENDS test_gcc_be_coro_large_frame)
add_test(NAME test_gcc_be_exec_coro_large_frame COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_coro_large_frame.exe)
set_tests_properties(test_gcc_be_exec_coro_large_frame PROPERTIES DEPENDS test_gcc_be_link_coro_large_frame)

add_test(NAME test_gcc_be_link_coro_large_frame_asan
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsanitize=address ${GCC_BE_EXEC_LINK_FLAGS} ${CMAKE_CURRENT_BINARY_DIR}/test_coro_large_frame.o $<TARGET_FILE:gcc_be_eh_rt> -o ${CMAKE_CURRENT_BINARY_DIR}/test_coro_large_frame_asan.exe)
set_tests_properties(test_gcc_be_link_coro_large_frame_asan PROPERTIES DEPENDS test_gcc_be_coro_large_frame)
add_test(NAME test_gcc_be_exec_coro_large_frame_asan COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_coro_large_frame_asan.exe)
set_tests_properties(test_gcc_be_exec_coro_large_frame_asan PROPERTIES DEPENDS test_gcc_be_link_coro_large_frame_asan)
