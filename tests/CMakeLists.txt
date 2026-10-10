cmake_minimum_required(VERSION 3.15)

project(EDG_Tests CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(test_llvm_type_lowering test_llvm_type_lowering.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp)

target_include_directories(test_llvm_type_lowering PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_type_lowering PRIVATE LLVM)

# We need the macro configs for BACK_END_IS_LLVM_GEN_BE=1 etc
target_compile_definitions(test_llvm_type_lowering PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_const_evaluation test_llvm_const_evaluation.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_const.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp)

target_include_directories(test_llvm_const_evaluation PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_const_evaluation PRIVATE LLVM)
target_compile_definitions(test_llvm_const_evaluation PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_expressions test_llvm_expressions.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_expr.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp)

target_include_directories(test_llvm_expressions PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_expressions PRIVATE LLVM)
target_compile_definitions(test_llvm_expressions PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_control_flow test_llvm_control_flow.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_stmt.cpp)

target_include_directories(test_llvm_control_flow PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_control_flow PRIVATE LLVM)
target_compile_definitions(test_llvm_control_flow PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_vtable_rtti test_llvm_vtable_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_vtable.cpp)

target_include_directories(test_llvm_vtable_rtti PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_vtable_rtti PRIVATE LLVM)
target_compile_definitions(test_llvm_vtable_rtti PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_exceptions test_llvm_exceptions.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_eh.cpp)

target_include_directories(test_llvm_exceptions PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_exceptions PRIVATE LLVM)
target_compile_definitions(test_llvm_exceptions PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_declarations test_llvm_declarations.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_decl.cpp)

target_include_directories(test_llvm_declarations PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_declarations PRIVATE LLVM)
target_compile_definitions(test_llvm_declarations PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_abi test_llvm_abi.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp)

target_include_directories(test_llvm_abi PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_abi PRIVATE LLVM)
target_compile_definitions(test_llvm_abi PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_inline_asm test_llvm_inline_asm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_asm.cpp)
target_include_directories(test_llvm_inline_asm PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_inline_asm PRIVATE LLVM)
target_compile_definitions(test_llvm_inline_asm PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_debug_info test_llvm_debug_info.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug_scopes.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug_types.cpp)

target_include_directories(test_llvm_debug_info PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_debug_info PRIVATE LLVM)
target_compile_definitions(test_llvm_debug_info PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_coro_atomics test_llvm_coro_atomics.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_coro.cpp)
target_include_directories(test_llvm_coro_atomics PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_coro_atomics PRIVATE LLVM)
target_compile_definitions(test_llvm_coro_atomics PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_opt_pipeline test_llvm_opt_pipeline.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_opt.cpp)
target_include_directories(test_llvm_opt_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_opt_pipeline PRIVATE LLVM)
target_compile_definitions(test_llvm_opt_pipeline PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_codegen test_llvm_codegen.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_codegen.cpp)
target_include_directories(test_llvm_codegen PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_codegen PRIVATE LLVM)
target_compile_definitions(test_llvm_codegen PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_builtins test_llvm_builtins.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_expr.cpp)
target_include_directories(test_llvm_builtins PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_builtins PRIVATE LLVM)
target_compile_definitions(test_llvm_builtins PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
