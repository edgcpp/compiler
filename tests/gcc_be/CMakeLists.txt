# CMakeLists.txt for gcc_be tests

option(EDG_GCC_BE_ENABLE_COVERAGE "Enable code coverage flags for GCC backend" OFF)
add_compile_options(-Wunused-result -Werror=unused-result)
if(EDG_GCC_BE_ENABLE_COVERAGE)
    add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-inline)
    add_link_options(--coverage)
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
)

add_custom_target(run_gcc_be_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running GCC Backend tests"
)

foreach(test_file ${GCC_BE_TEST_FILES})
    set(EXTRA_FLAGS "")
    if(test_file STREQUAL "test_virtual.cpp")
        set(EXTRA_FLAGS "--c++11" "--no_exceptions" "--sys_include" "${PROJECT_SOURCE_DIR}/include_c++")
    elseif(test_file STREQUAL "test_eh.cpp")
        set(EXTRA_FLAGS "--c++11" "--exceptions" "--sys_include" "${PROJECT_SOURCE_DIR}/include_c++")
    elseif(test_file MATCHES "\\.cpp$")
        set(EXTRA_FLAGS "--c++" "--no_exceptions")
    elseif(test_file STREQUAL "test_aggregates.c")
        set(EXTRA_FLAGS "--c" "--c99")
    elseif(test_file STREQUAL "test_edge_cases.c" OR test_file STREQUAL "test_control_flow.c")
        set(EXTRA_FLAGS "--c" "--gcc")
    elseif(test_file MATCHES "\\.c$")
        set(EXTRA_FLAGS "--c" "--c99")
    endif()

    add_test(NAME test_gcc_be_${test_file} 
             COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be ${EXTRA_FLAGS} -I${PROJECT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR} -c -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o ${CMAKE_CURRENT_SOURCE_DIR}/${test_file})
    set_tests_properties(test_gcc_be_${test_file} PROPERTIES 
        FIXTURES_SETUP "gcc_be_test_run"
        ENVIRONMENT "EDG_BASE=${PROJECT_SOURCE_DIR}/bases/cmake-native/macos-arm/clang")

    if(test_file MATCHES "\\.cpp$")
        add_test(NAME test_gcc_be_link_${test_file}
                 COMMAND ${CMAKE_CXX_COMPILER} -std=c++14 -I${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o ${CMAKE_CURRENT_SOURCE_DIR}/gcc_be_eh_rt.cpp -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.exe)
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
add_test(NAME test_gcc_be_coverage COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check_coverage.py)
set_tests_properties(test_gcc_be_coverage PROPERTIES FIXTURES_REQUIRED "gcc_be_test_run")

add_test(NAME test_gcc_be_doc_coverage COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/dev_tools/check_gcc_be_doc_coverage.py)

add_custom_target(coverage_gcc_be
    COMMAND ${CMAKE_CTEST_COMMAND} -R test_gcc_be_coverage --output-on-failure
)

# Unit tests for backend components
add_executable(test_error test_error.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_error error-info)
target_include_directories(test_error PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_options(test_error PRIVATE "-x" "c++")
target_compile_definitions(test_error PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
add_test(NAME test_error_unit COMMAND test_error)

add_executable(test_gcc_be_errors test_gcc_be_errors.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_gcc_be_errors error-info)
target_include_directories(test_gcc_be_errors PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_errors_unit COMMAND test_gcc_be_errors)
target_compile_options(test_gcc_be_errors PRIVATE "-x" "c++")
target_compile_definitions(test_gcc_be_errors PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_include_directories(test_gcc_be_errors PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)

add_executable(test_gcc_be_context test_gcc_be_context.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_context error-info)
target_include_directories(test_gcc_be_context PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_gcc_be_context PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_context_unit COMMAND test_gcc_be_context)
target_compile_options(test_gcc_be_context PRIVATE "-x" "c++")
target_compile_definitions(test_gcc_be_context PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_include_directories(test_gcc_be_context PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)

add_executable(test_gcc_be_lib_loader test_gcc_be_lib_loader.cpp)
add_dependencies(test_gcc_be_lib_loader error-info)
target_include_directories(test_gcc_be_lib_loader PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_lib_loader_unit COMMAND test_gcc_be_lib_loader)
target_compile_definitions(test_gcc_be_lib_loader PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_lib_loader PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_lib_loader PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)

add_executable(test_gcc_be_cache test_gcc_be_cache.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c)
add_dependencies(test_gcc_be_cache error-info)
target_include_directories(test_gcc_be_cache PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_cache_unit COMMAND test_gcc_be_cache)
target_compile_definitions(test_gcc_be_cache PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_cache PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_cache PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)

add_executable(test_gcc_be_type test_gcc_be_type.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_type error-info)
target_include_directories(test_gcc_be_type PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_type_unit COMMAND test_gcc_be_type)
target_compile_definitions(test_gcc_be_type PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_type PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_type PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_type PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_rtti test_gcc_be_rtti.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_rtti error-info)
target_include_directories(test_gcc_be_rtti PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_rtti_unit COMMAND test_gcc_be_rtti)
target_compile_definitions(test_gcc_be_rtti PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_rtti PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_rtti PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_rtti PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_builtin test_gcc_be_builtin.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_builtin.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_builtin error-info)
target_include_directories(test_gcc_be_builtin PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_builtin_unit COMMAND test_gcc_be_builtin)
target_compile_definitions(test_gcc_be_builtin PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_builtin PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_builtin PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_builtin PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_decl test_gcc_be_decl.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_decl.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c)
add_dependencies(test_gcc_be_decl error-info)
target_include_directories(test_gcc_be_decl PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_decl_unit COMMAND test_gcc_be_decl)
target_compile_definitions(test_gcc_be_decl PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_decl PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_decl PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_decl PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_expr test_gcc_be_expr.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_expr error-info)
target_include_directories(test_gcc_be_expr PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_expr_unit COMMAND test_gcc_be_expr)
target_compile_definitions(test_gcc_be_expr PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_expr PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_expr PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_expr PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_abi test_gcc_be_abi.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_abi.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_abi error-info)
target_include_directories(test_gcc_be_abi PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_abi_unit COMMAND test_gcc_be_abi)
target_compile_definitions(test_gcc_be_abi PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_abi PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_abi PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_abi PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_debug test_gcc_be_debug.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_debug.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_debug error-info)
target_include_directories(test_gcc_be_debug PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_debug_unit COMMAND test_gcc_be_debug)
target_compile_definitions(test_gcc_be_debug PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_debug PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_debug PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_debug PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_dtor test_gcc_be_dtor.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_dtor.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_dtor error-info)
target_include_directories(test_gcc_be_dtor PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_dtor_unit COMMAND test_gcc_be_dtor)
target_compile_definitions(test_gcc_be_dtor PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_dtor PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_dtor PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_dtor PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_stmt2 test_gcc_be_stmt2.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_stmt2 error-info)
target_include_directories(test_gcc_be_stmt2 PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_stmt2_unit COMMAND test_gcc_be_stmt2)
target_compile_definitions(test_gcc_be_stmt2 PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_stmt2 PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_stmt2 PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_stmt2 PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_stmt test_gcc_be_stmt.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_stmt error-info)
target_include_directories(test_gcc_be_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_stmt_unit COMMAND test_gcc_be_stmt)
target_compile_definitions(test_gcc_be_stmt PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_stmt PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_stmt PRIVATE ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_link_libraries(test_gcc_be_stmt PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_error test_gcc_be_error.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_gcc_be_error error-info)
target_include_directories(test_gcc_be_error PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_error PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_error PRIVATE "-x" "c++")
add_test(NAME test_gcc_be_error_unit COMMAND test_gcc_be_error)

add_executable(test_gcc_be_loader test_gcc_be_loader.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_loader error-info)
target_include_directories(test_gcc_be_loader PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_loader PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_loader PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_loader PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_loader_unit COMMAND test_gcc_be_loader)

add_executable(test_gcc_be_coroutines test_gcc_be_coroutines.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_coroutines error-info)
target_include_directories(test_gcc_be_coroutines PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_coroutines PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_coroutines PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_coroutines PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_coroutines_unit COMMAND test_gcc_be_coroutines)

add_executable(test_gcc_be_abi_targets test_gcc_be_abi_targets.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_abi.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_abi_targets error-info)
target_include_directories(test_gcc_be_abi_targets PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_abi_targets PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_abi_targets PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_abi_targets PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_abi_targets_unit COMMAND test_gcc_be_abi_targets)

add_executable(test_gcc_be_bitfields test_gcc_be_bitfields.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_bitfields error-info)
target_include_directories(test_gcc_be_bitfields PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_bitfields PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_bitfields PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_bitfields PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_bitfields_unit COMMAND test_gcc_be_bitfields)

add_executable(test_gcc_be_rtti_full test_gcc_be_rtti_full.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_rtti_full error-info)
target_include_directories(test_gcc_be_rtti_full PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_rtti_full PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_rtti_full PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_rtti_full PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_rtti_full_unit COMMAND test_gcc_be_rtti_full)

add_executable(test_gcc_be_eh test_gcc_be_eh.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_eh.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_eh error-info)
target_include_directories(test_gcc_be_eh PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_eh PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_eh PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_eh PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_eh_unit COMMAND test_gcc_be_eh)

add_executable(test_gcc_be_eh_dwarf test_gcc_be_eh_dwarf.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_eh.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_eh_dwarf error-info)
target_include_directories(test_gcc_be_eh_dwarf PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_eh_dwarf PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_eh_dwarf PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_eh_dwarf PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_eh_dwarf_unit COMMAND test_gcc_be_eh_dwarf)

add_executable(test_gcc_be_simd test_gcc_be_simd.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_builtin.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_simd error-info)
target_include_directories(test_gcc_be_simd PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_simd PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_simd PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_simd PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_simd_unit COMMAND test_gcc_be_simd)

add_executable(test_gcc_be_debug_dwarf test_gcc_be_debug_dwarf.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_debug.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_debug_dwarf error-info)
target_include_directories(test_gcc_be_debug_dwarf PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_debug_dwarf PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_debug_dwarf PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_debug_dwarf PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_debug_dwarf_unit COMMAND test_gcc_be_debug_dwarf)

add_executable(test_gcc_be_main test_gcc_be_main.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_main.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_main error-info)
target_include_directories(test_gcc_be_main PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_main PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_main PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_main PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_main_unit COMMAND test_gcc_be_main)

add_executable(test_gcc_be_driver test_gcc_be_driver.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_main.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_driver error-info)
target_include_directories(test_gcc_be_driver PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_driver PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_driver PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_driver PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_driver_unit COMMAND test_gcc_be_driver)

add_executable(test_gcc_be_location test_gcc_be_location.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
add_dependencies(test_gcc_be_location error-info)
target_include_directories(test_gcc_be_location PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_location PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_location PRIVATE "-x" "c++")
target_link_libraries(test_gcc_be_location PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_location_unit COMMAND test_gcc_be_location)

add_executable(test_gcc_be_regression test_gcc_be_regression.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
add_dependencies(test_gcc_be_regression error-info)
target_include_directories(test_gcc_be_regression PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/includes ${PROJECT_BINARY_DIR}/src/includes)
target_compile_definitions(test_gcc_be_regression PRIVATE BACK_END_IS_GCC_GEN_BE=1 USE_CMAKE_DEFINES=1 DO_IL_LOWERING=1 USE_EDG_NAMESPACE=1)
target_compile_options(test_gcc_be_regression PRIVATE "-x" "c++")
add_test(NAME test_gcc_be_regression_unit COMMAND test_gcc_be_regression)
