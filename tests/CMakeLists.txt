cmake_minimum_required(VERSION 3.15)

project(EDG_Tests CXX)

set(CMAKE_CXX_STANDARD 17)

if(NOT MSVC)
  add_compile_options(-UNDEBUG)
endif()

option(EDG_LLVM_BE_COVERAGE "Enable Clang source-based code coverage for LLVM backend tests" ON)
if(EDG_LLVM_BE_COVERAGE AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
  add_link_options(-fprofile-instr-generate -fcoverage-mapping)
endif()

find_package(LLVM REQUIRED CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_BINARY_DIR}/includes ${CMAKE_BINARY_DIR}/src/includes)
add_definitions(${LLVM_DEFINITIONS})
add_compile_definitions(IA64_ABI=1 FIXED_POINT_ALLOWED=0 LOWER_FIXED_POINT=0 NAMED_ADDRESS_SPACES_ALLOWED=0 MICROSOFT_EXTENSIONS_ALLOWED=1 RECORD_RAW_ASM_OPERAND_DESCRIPTIONS=1 CHECKING=0 EXPENSIVE_CHECKING=0)

add_executable(test_llvm_type_lowering test_llvm_type_lowering.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_x86_32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_arm32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_wasm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_riscv64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_loongarch64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_mips.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_ppc64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_s390x.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sparc.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp)

target_include_directories(test_llvm_type_lowering PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_type_lowering PRIVATE LLVM)

# We need the macro configs for BACK_END_IS_LLVM_GEN_BE=1 etc
target_compile_definitions(test_llvm_type_lowering PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_const_evaluation test_llvm_const_evaluation.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_const.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_x86_32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_arm32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_wasm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_riscv64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_loongarch64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_mips.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_ppc64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_s390x.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sparc.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)

target_include_directories(test_llvm_const_evaluation PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_const_evaluation PRIVATE LLVM)
target_compile_definitions(test_llvm_const_evaluation PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_expressions test_llvm_expressions.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_expr.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_intrinsics.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_x86_32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_arm32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_wasm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_riscv64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_loongarch64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_mips.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_ppc64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_s390x.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sparc.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_vtable.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_coro.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)

target_include_directories(test_llvm_expressions PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_expressions PRIVATE LLVM)
target_compile_definitions(test_llvm_expressions PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_control_flow test_llvm_control_flow.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_stmt.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_asm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_coro.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_eh.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_omp.cpp)

target_include_directories(test_llvm_control_flow PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_control_flow PRIVATE LLVM)
target_compile_definitions(test_llvm_control_flow PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_vtable_rtti test_llvm_vtable_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_vtable.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp)

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

add_executable(test_llvm_declarations test_llvm_declarations.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_decl.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_stmt.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_eh.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_omp.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)

target_include_directories(test_llvm_declarations PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_declarations PRIVATE LLVM)
target_compile_definitions(test_llvm_declarations PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_lto test_llvm_lto.cpp)

target_include_directories(test_llvm_lto PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include_c++
  ${CMAKE_BINARY_DIR}/src/includes
)

target_link_libraries(test_llvm_lto PRIVATE LLVM)
target_compile_definitions(test_llvm_lto PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_pgo test_llvm_pgo.cpp)
target_include_directories(test_llvm_pgo PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_llvm_pgo PRIVATE LLVM)
target_compile_definitions(test_llvm_pgo PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_abi test_llvm_abi.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_riscv64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_loongarch64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_mips.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_ppc64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_s390x.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sparc.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp
  ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_x86_32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_arm32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_wasm.cpp)

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

add_executable(test_llvm_msvc_asm test_llvm_msvc_asm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_asm.cpp)
target_include_directories(test_llvm_msvc_asm PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_msvc_asm PRIVATE LLVM)
target_compile_definitions(test_llvm_msvc_asm PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

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

add_executable(test_llvm_builtins test_llvm_builtins.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_expr.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_intrinsics.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_x86_32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_arm32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_wasm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_riscv64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_loongarch64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_mips.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_ppc64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_s390x.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sparc.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_vtable.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_coro.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(test_llvm_builtins PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_builtins PRIVATE LLVM)
target_compile_definitions(test_llvm_builtins PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_target_intrinsics test_llvm_target_intrinsics.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_intrinsics.cpp)
target_include_directories(test_llvm_target_intrinsics PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_target_intrinsics PRIVATE LLVM)
target_compile_definitions(test_llvm_target_intrinsics PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_error test_llvm_error.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(test_llvm_error PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_error PRIVATE LLVM)
target_compile_definitions(test_llvm_error PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_orchestrator test_llvm_orchestrator.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_driver.cpp)
target_include_directories(test_llvm_orchestrator PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_orchestrator PRIVATE LLVM)
target_compile_definitions(test_llvm_orchestrator PRIVATE BACK_END_IS_LLVM_GEN_BE=1 MAKE_FRONT_END_CALLABLE=1)
add_executable(test_llvm_decl test_llvm_decl.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_decl.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_x86_32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_arm32.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_wasm.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sysv_x86_64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_aapcs64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_riscv64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_loongarch64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_mips.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_ppc64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_s390x.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_sparc.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_rtti.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_ptrmem.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_stmt.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_eh.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_omp.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(test_llvm_decl PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_decl PRIVATE LLVM)
target_compile_definitions(test_llvm_decl PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_stmt llvm_ir_backend_tests/stmt_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_stmt.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_eh.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_omp.cpp)
target_include_directories(test_llvm_stmt PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_stmt PRIVATE LLVM)
target_compile_definitions(test_llvm_stmt PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_omp test_llvm_omp.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_omp.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_stmt.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_eh.cpp)
target_include_directories(test_llvm_omp PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_omp PRIVATE LLVM)
target_compile_definitions(test_llvm_omp PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_datalayout test_llvm_datalayout.cpp)
target_include_directories(test_llvm_datalayout PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_compile_definitions(test_llvm_datalayout PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_type_translation test_llvm_type_translation.cpp)
target_include_directories(test_llvm_type_translation PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_compile_definitions(test_llvm_type_translation PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_verifier test_llvm_verifier.cpp)
target_include_directories(test_llvm_verifier PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_compile_definitions(test_llvm_verifier PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_driver_flag test_llvm_driver_flag.cpp)
target_include_directories(test_llvm_driver_flag PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_compile_definitions(test_llvm_driver_flag PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_fuzz test_llvm_fuzz.cpp)
target_include_directories(test_llvm_fuzz PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_compile_definitions(test_llvm_fuzz PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_build_config test_llvm_build_config.cpp)
target_include_directories(test_llvm_build_config PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_compile_definitions(test_llvm_build_config PRIVATE BACK_END_IS_LLVM_GEN_BE=1)

add_executable(test_llvm_e2e_execution test_llvm_e2e_execution.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(test_llvm_e2e_execution PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include_c++ ${CMAKE_BINARY_DIR}/src/includes)
target_link_libraries(test_llvm_e2e_execution PRIVATE LLVM ${CMAKE_DL_LIBS})
target_compile_definitions(test_llvm_e2e_execution PRIVATE BACK_END_IS_LLVM_GEN_BE=1 STANDALONE_UTILITY_PROGRAM=1)

foreach(llvm_test_target
    test_llvm_type_lowering
    test_llvm_const_evaluation
    test_llvm_expressions
    test_llvm_control_flow
    test_llvm_exceptions
    test_llvm_declarations
    test_llvm_abi
    test_llvm_debug_info
    test_llvm_codegen
    test_llvm_opt_pipeline
    test_llvm_pgo
    test_llvm_lto
    test_llvm_vtable_rtti
    test_llvm_coro_atomics
    test_llvm_inline_asm
    test_llvm_msvc_asm
    test_llvm_builtins
    test_llvm_target_intrinsics
    test_llvm_error
    test_llvm_orchestrator
    test_llvm_decl
    test_llvm_stmt
    test_llvm_omp
    test_llvm_datalayout
    test_llvm_type_translation
    test_llvm_verifier
    test_llvm_driver_flag
    test_llvm_fuzz
    test_llvm_build_config
    test_llvm_e2e_execution)
  if(TARGET ${llvm_test_target})
    if(TARGET error-info)
      add_dependencies(${llvm_test_target} error-info)
    endif()
    add_test(NAME ${llvm_test_target} COMMAND ${llvm_test_target})
  endif()
endforeach()
