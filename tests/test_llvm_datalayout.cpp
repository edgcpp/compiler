/**
 * @file test_llvm_datalayout.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for LLVM DataLayout string construction from target configurations.
 * @details Validates endianness, pointer width, integer width, and floating-point alignment strings.
 */

#include "llvm_gen_be_error.h"
#include <iostream>
#include <string>
#include <cassert>

using namespace edg;

/**
 * @struct TargetConfig
 * @brief Represents target bit-width and endianness parameters for DataLayout generation.
 * @details Models EDG `targ_*` sizing globals across 32-bit, 64-bit, little-endian, and big-endian targets.
 */
struct TargetConfig {
    int char_bit;        ///< Bits per byte (`targ_char_bit`).
    int short_bit;       ///< Bits in `short`.
    int int_bit;         ///< Bits in `int`.
    int long_long_bit;   ///< Bits in `long long`.
    int float_bit;       ///< Bits in `float`.
    int double_bit;      ///< Bits in `double`.
    int long_double_bit; ///< Bits in `long double`.
    int pointer_size;    ///< Size of pointer in bytes (`targ_sizeof_pointer`).
    bool little_endian;  ///< True if target is little-endian (`targ_little_endian`).
};

/**
 * @brief Builds an LLVM DataLayout string from `targ` and stores it in `out_dl`.
 * @details Formats endianness, pointer size, integer sizes, and float sizes.
 * @param[in] targ The target configuration structure.
 * @param[out] out_dl Pointer to the output string receiving the DataLayout specification.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_dl` is null.
 */
llvm_gen_be_error_t mock_build_data_layout(const TargetConfig& targ, std::string* out_dl) noexcept {
    if (!out_dl) return llvm_gen_be_error_t::invalid_argument;
    std::string dl = "";
    if (targ.little_endian) {
        dl += "e-";
    } else {
        dl += "E-";
    }
    dl += "p:" + std::to_string(targ.pointer_size * targ.char_bit) + ":" + std::to_string(targ.pointer_size * targ.char_bit) + "-";
    dl += "i8:" + std::to_string(targ.char_bit) + "-";
    dl += "i16:" + std::to_string(targ.short_bit) + "-";
    dl += "i32:" + std::to_string(targ.int_bit) + "-";
    dl += "i64:" + std::to_string(targ.long_long_bit) + "-";
    dl += "f32:" + std::to_string(targ.float_bit) + "-";
    dl += "f64:" + std::to_string(targ.double_bit) + "-";
    dl += "f128:" + std::to_string(targ.long_double_bit);
    *out_dl = dl;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests DataLayout string generation for little-endian x86_64.
 * @details Verifies 64-bit pointers and 128-bit long double formatting.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_x86_64(void) noexcept {
    TargetConfig config = {8, 16, 32, 64, 32, 64, 128, 8, true};
    std::string dl;
    LLVM_GEN_BE_CHECK(mock_build_data_layout(config, &dl));
    assert(dl == "e-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:128");
    std::cout << "x86_64 test passed: " << dl << "\n";
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests DataLayout string generation for little-endian 32-bit ARM.
 * @details Verifies 32-bit pointers and 64-bit long double formatting.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_arm_32(void) noexcept {
    TargetConfig config = {8, 16, 32, 64, 32, 64, 64, 4, true};
    std::string dl;
    LLVM_GEN_BE_CHECK(mock_build_data_layout(config, &dl));
    assert(dl == "e-p:32:32-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:64");
    std::cout << "arm_32 test passed: " << dl << "\n";
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests DataLayout string generation for big-endian 64-bit targets.
 * @details Verifies `E-` prefix and 64-bit pointer layout.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_big_endian_64(void) noexcept {
    TargetConfig config = {8, 16, 32, 64, 32, 64, 128, 8, false};
    std::string dl;
    LLVM_GEN_BE_CHECK(mock_build_data_layout(config, &dl));
    assert(dl == "E-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:128");
    std::cout << "big_endian_64 test passed: " << dl << "\n";
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests DataLayout string generation for LLP64 Windows and ILP32 x86 targets.
 * @details Verifies 64-bit pointer with 64-bit long double on LLP64 and 32-bit pointer with 96-bit long double on i386.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_llp64_and_ilp32(void) noexcept {
    TargetConfig win64_cfg = {8, 16, 32, 64, 32, 64, 64, 8, true};
    std::string dl_win64;
    LLVM_GEN_BE_CHECK(mock_build_data_layout(win64_cfg, &dl_win64));
    assert(dl_win64 == "e-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:64");

    TargetConfig i386_cfg = {8, 16, 32, 64, 32, 64, 96, 4, true};
    std::string dl_i386;
    LLVM_GEN_BE_CHECK(mock_build_data_layout(i386_cfg, &dl_i386));
    assert(dl_i386 == "e-p:32:32-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:96");
    assert(mock_build_data_layout(i386_cfg, nullptr) == llvm_gen_be_error_t::invalid_argument);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the DataLayout unit test suite.
 * @details Executes x86_64, ARM32, big-endian 64-bit, LLP64, and ILP32 DataLayout tests.
 * @return 0 on success, or non-zero on error.
 */
int main(void) {
    if (test_x86_64() != llvm_gen_be_error_t::ok) return 1;
    if (test_arm_32() != llvm_gen_be_error_t::ok) return 1;
    if (test_big_endian_64() != llvm_gen_be_error_t::ok) return 1;
    if (test_llp64_and_ilp32() != llvm_gen_be_error_t::ok) return 1;
    std::cout << "All DataLayout tests passed!\n";
    return 0;
}
