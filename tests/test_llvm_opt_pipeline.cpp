/**
 * @file test_llvm_opt_pipeline.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend optimization pipeline.
 * @details Validates function coverage of llvm_gen_be_opt.cpp.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_opt.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ProfileData/InstrProfWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  // Stubs
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_opt_pipeline.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_opt_pipeline.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_opt_pipeline.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);
  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_opt_pipeline.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_opt_pipeline.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_opt_pipeline.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_opt_pipeline.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  a_const_char* fprofile_generate_path = nullptr;
}

/**
 * @brief Entry point for the LLVM New Pass Manager optimization pipeline unit tests.
 * @details Exercises `parse_opt_level_string` and `run_optimization_pipeline` across opt levels and sanitizers.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_opt_pipeline...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_opt", *state.context);
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "decl_only_func", state.module.get());
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);
  state.builder->CreateRetVoid();

  llvm_opt_options_t opts = {};
  assert(parse_opt_level_string(nullptr, &opts) == llvm_gen_be_error_t::invalid_argument);
  assert(parse_opt_level_string("O0", nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(run_optimization_pipeline(nullptr, &opts) == llvm_gen_be_error_t::invalid_argument);
  assert(run_optimization_pipeline(state.module.get(), nullptr) == llvm_gen_be_error_t::invalid_argument);

  assert(parse_opt_level_string("O0", &opts) == llvm_gen_be_error_t::ok);
  assert(opts.opt_level == llvm_opt_level_t::O0);
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  assert(parse_opt_level_string("O1", &opts) == llvm_gen_be_error_t::ok);
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  assert(parse_opt_level_string("O2", &opts) == llvm_gen_be_error_t::ok);
  opts.use_msan = true;
  opts.pgo_generate = true;
  fprofile_generate_path = nullptr;
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);
  fprofile_generate_path = "/tmp/edg_opt_test.profraw";
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);
  opts.pgo_generate = false;
  opts.use_msan = false;

  // Test pgo_use_path with a valid indexed profdata file
  {
    llvm::InstrProfWriter writer;
    llvm::consumeError(writer.mergeProfileKind(llvm::InstrProfKind::IRInstrumentation));
    std::error_code ec;
    llvm::raw_fd_ostream prof_out("/tmp/edg_opt_use.profdata", ec, llvm::sys::fs::OF_None);
    assert(!ec);
    llvm::consumeError(writer.write(prof_out));
    prof_out.close();

    opts.pgo_use_path = "/tmp/edg_opt_use.profdata";
    assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);
    opts.pgo_use_path = nullptr;
    std::remove("/tmp/edg_opt_use.profdata");
  }

  assert(parse_opt_level_string("O3", &opts) == llvm_gen_be_error_t::ok);
  opts.use_asan = true;
  opts.use_tsan = true;
  opts.use_ubsan = true;
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);
  
  assert(parse_opt_level_string("Os", &opts) == llvm_gen_be_error_t::ok);
  opts.use_lto = true;
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  assert(parse_opt_level_string("Oz", &opts) == llvm_gen_be_error_t::ok);
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  // Default opt_level enum branch
  opts.opt_level = static_cast<llvm_opt_level_t>(99);
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  // OOM error propagation + O0 coroutine lowering when llvm.coro.id is declared
  assert(parse_opt_level_string("O0", &opts) == llvm_gen_be_error_t::ok);
  for (int b = 0; b < 4; ++b) {
    g_llvm_be_oom_countdown = b;
    llvm_gen_be_error_t ropt = run_optimization_pipeline(state.module.get(), &opts);
    g_llvm_be_oom_countdown = -1;
    if (ropt == llvm_gen_be_error_t::ok) break;
  }
  EDG_LLVM_GET_INTRINSIC_DECL(state.module.get(), llvm::Intrinsic::coro_id);
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  assert(parse_opt_level_string("invalid", &opts) == llvm_gen_be_error_t::invalid_argument);

  printf("PASS\n");
  return 0;
}
