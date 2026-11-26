//type:cp
//options_all:--microsoft_v 1950 --ms_c++20
constexpr int square(int x) {
    if consteval {
        return x * x + 100;
    } else {
        return x * x;
    }
}

constexpr int val = square(5);
