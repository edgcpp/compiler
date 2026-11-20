//type:cp
//options_all:--c++20
constexpr float getFloat() {
    unsigned short x = 359;
    const unsigned short divisor = 20;
    const float step = x / (float)divisor;
    for (short i = 0; i < divisor; i++) {
        x -= step;
    }
    return x;
}

int main() {
    constexpr auto x = getFloat();
    return x;
}
