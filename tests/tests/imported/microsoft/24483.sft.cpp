//options_all:--c++20
template <class C, unsigned int N>
struct fixed_string {
    C data[N];

    constexpr fixed_string(const C (&str)[N]) {
        for (auto i = 0u; i < N; i++) {
            data[i] = str[i];
        }
    }
};

template <fixed_string>
constexpr int operator""_cf() { return 3; }

static_assert("abc"_cf == 3);
