//type:fp
//options_all:--microsoft_version=1914 --ms_c++17
int main() {
    constexpr double CorrectDouble     = 0x1.fffffdp0;
    constexpr float  CorrectFloat      = 0x1.fffffep0f;
    constexpr float  TwiceRoundedFloat = 0x1.fffffcp0f;
    static_assert(static_cast<float>(CorrectDouble) == TwiceRoundedFloat,
        "static_cast<float>(CorrectDouble) == TwiceRoundedFloat");

    constexpr double decimal_double = 1.999999821186065729339276231257827021181583404541015625;
    constexpr float  decimal_float  = 1.999999821186065729339276231257827021181583404541015625f;

    static_assert(decimal_double == CorrectDouble,    "decimal_double == CorrectDouble");
    static_assert(decimal_float == CorrectFloat,      "decimal_float == CorrectFloat");
    static_assert(decimal_float != TwiceRoundedFloat, "decimal_float != TwiceRoundedFloat");

    constexpr double hex_double = 0x1.fffffd00000004p0;
    constexpr float  hex_float  = 0x1.fffffd00000004p0f;

    static_assert(hex_double == CorrectDouble,    "hex_double == CorrectDouble");
    static_assert(hex_float == CorrectFloat,      "hex_float == CorrectFloat");
    static_assert(hex_float != TwiceRoundedFloat, "hex_float != TwiceRoundedFloat");
}
