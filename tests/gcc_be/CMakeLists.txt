# CMakeLists.txt for gcc_be tests

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
)

add_custom_target(run_gcc_be_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running GCC Backend tests"
)

foreach(test_file ${GCC_BE_TEST_FILES})
    # Determine if it's C or C++
    if(test_file MATCHES "\\.cpp$")
        set(TEST_LANG "CXX")
    else()
        set(TEST_LANG "C")
    endif()

    add_test(NAME test_gcc_be_${test_file} 
             COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be -c ${CMAKE_CURRENT_SOURCE_DIR}/${test_file} -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o)
    set_tests_properties(test_gcc_be_${test_file} PROPERTIES FIXTURES_SETUP "gcc_be_test_run")
endforeach()

add_test(NAME test_gcc_be_coverage COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_coverage.py)
set_tests_properties(test_gcc_be_coverage PROPERTIES FIXTURES_REQUIRED "gcc_be_test_run")

add_custom_target(coverage_gcc_be
    COMMAND ${CMAKE_CTEST_COMMAND} -R test_gcc_be_coverage --output-on-failure
)

# Unit tests for backend components
add_executable(test_gcc_be_errors test_gcc_be_errors.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
target_include_directories(test_gcc_be_errors PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_errors_unit COMMAND test_gcc_be_errors)
target_compile_options(test_gcc_be_errors PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_errors PRIVATE ${PROJECT_BINARY_DIR}/includes)
add_dependencies(test_gcc_be_errors error-info)

add_executable(test_gcc_be_context test_gcc_be_context.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c)
target_include_directories(test_gcc_be_context PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_context_unit COMMAND test_gcc_be_context)
target_compile_options(test_gcc_be_context PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_context PRIVATE ${PROJECT_BINARY_DIR}/includes)
add_dependencies(test_gcc_be_context error-info)

add_executable(test_gcc_be_lib_loader test_gcc_be_lib_loader.cpp)
target_include_directories(test_gcc_be_lib_loader PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_lib_loader_unit COMMAND test_gcc_be_lib_loader)
target_compile_definitions(test_gcc_be_lib_loader PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_lib_loader PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_lib_loader PRIVATE ${PROJECT_BINARY_DIR}/includes)

add_executable(test_gcc_be_cache test_gcc_be_cache.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c)
target_include_directories(test_gcc_be_cache PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_cache_unit COMMAND test_gcc_be_cache)
target_compile_definitions(test_gcc_be_cache PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_cache PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_cache PRIVATE ${PROJECT_BINARY_DIR}/includes)

add_executable(test_gcc_be_type test_gcc_be_type.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_type PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_type_unit COMMAND test_gcc_be_type)
target_compile_definitions(test_gcc_be_type PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_type PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_type PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_type PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_decl test_gcc_be_decl.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_decl.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_decl PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_decl_unit COMMAND test_gcc_be_decl)
target_compile_definitions(test_gcc_be_decl PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_decl PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_decl PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_decl PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_expr test_gcc_be_expr.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_expr PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_expr_unit COMMAND test_gcc_be_expr)
target_compile_definitions(test_gcc_be_expr PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_expr PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_expr PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_expr PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_stmt test_gcc_be_stmt.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_stmt_unit COMMAND test_gcc_be_stmt)
target_compile_definitions(test_gcc_be_stmt PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_stmt PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_stmt PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_stmt PRIVATE LibGCCJIT::LibGCCJIT)
