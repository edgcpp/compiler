//options:--microsoft_v 1930 --ms_c++20 --ms_cplusplus_std_value:--gn 120100 --c++20:--clang_v 140000 --c++20;fn
#include <compare>

template <typename T>
struct wrap {
    T data;

    template <typename U>
    auto operator<=>(const wrap<U>& other) {
        return data <=> other.data;
    }
};

int main() {
    wrap<double> a{ 1.0 };
    wrap<float> b{ 2.0f };
    a <=> b;
}
