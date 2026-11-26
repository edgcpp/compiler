//options_all:--c++20
using size_t = decltype(sizeof(0));

template <size_t N>
struct fixed_string {
    char str[N] = {};

    constexpr fixed_string(char const (&arr)[N]) {
        for (size_t i = 0; i < N; ++i)
            str[i] = arr[i];
    }
};

template <fixed_string str>
constexpr auto operator""_fixed_str() {
    return str;
}

constexpr auto string_test = "abc"_fixed_str;
