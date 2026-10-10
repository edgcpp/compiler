#include <iostream>
#include <string>
#include <cassert>

// Mocking EDG macros for different targets
struct TargetConfig {
    int char_bit;
    int short_bit;
    int int_bit;
    int long_long_bit;
    int float_bit;
    int double_bit;
    int long_double_bit;
    int pointer_size;
    bool little_endian;
};

std::string mock_build_data_layout(const TargetConfig& targ) {
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
    return dl;
}

void test_x86_64() {
    TargetConfig config = {8, 16, 32, 64, 32, 64, 128, 8, true};
    std::string dl = mock_build_data_layout(config);
    assert(dl == "e-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:128");
    std::cout << "x86_64 test passed: " << dl << "\n";
}

void test_arm_32() {
    TargetConfig config = {8, 16, 32, 64, 32, 64, 64, 4, true};
    std::string dl = mock_build_data_layout(config);
    assert(dl == "e-p:32:32-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:64");
    std::cout << "arm_32 test passed: " << dl << "\n";
}

void test_big_endian_64() {
    TargetConfig config = {8, 16, 32, 64, 32, 64, 128, 8, false};
    std::string dl = mock_build_data_layout(config);
    assert(dl == "E-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:128");
    std::cout << "big_endian_64 test passed: " << dl << "\n";
}

int main() {
    test_x86_64();
    test_arm_32();
    test_big_endian_64();
    std::cout << "All DataLayout tests passed!\n";
    return 0;
}
